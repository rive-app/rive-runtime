/*
 * Copyright 2026 Rive
 */

#include <limits>
#include "rive/artboard.hpp"
#include "rive/bitmap_cache.hpp"
#include "rive/core/vector_binary_writer.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/layout/n_slicer.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/nested_artboard_layout.hpp"
#include "rive/nested_artboard_leaf.hpp"
#include "rive/nested_artboard_mesh_host.hpp"
#include "rive/node.hpp"
#include "rive/offscreen_raster.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/cmd/render_commands.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/shapes/mesh.hpp"
#include "rive/shapes/clipping_shape.hpp"
#include "rive/shapes/mesh_host.hpp"
#include "rive/shapes/mesh_vertex.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/shapes/shape.hpp"
#include "common/render_context_null.hpp"

#include <catch.hpp>
#include <functional>
#include <vector>

using namespace rive;
using namespace rive::cmd;

namespace
{
class CountingSession : public DeferredSession
{
public:
    using DeferredSession::DeferredSession;
    struct Raster
    {
        uint32_t width;
        uint32_t height;
    };
    std::vector<Raster> rasters;
    // Stands in for a host whose composite renderer starts without clips.
    bool dropsClips = false;
    bool compositeDropsClips() const override { return dropsClips; }

    Renderer* beginCanvasContent(gpu::RenderCanvas* canvas,
                                 uint32_t clearColor) override
    {
        rasters.push_back({canvas->width(), canvas->height()});
        return DeferredSession::beginCanvasContent(canvas, clearColor);
    }
    // Runs after the content draws, inside the raster pass.
    std::function<void()> onContentDrawn;
    void endCanvasContent(gpu::RenderCanvas* canvas) override
    {
        DeferredSession::endCanvasContent(canvas);
        if (onContentDrawn)
        {
            onContentDrawn();
        }
    }
};

constexpr size_t kNumCmds = static_cast<size_t>(RenderCmd::lastRenderCmd) + 1;

struct Census
{
    uint64_t count[kNumCmds] = {};
    uint64_t of(RenderCmd cmd) const { return count[static_cast<size_t>(cmd)]; }
    static Census of(const RenderCommandBuffer& buf)
    {
        Census c;
        CommandReader<uint8_t> r(buf.commandBytes(), buf.blobBytes());
        uint8_t type;
        while (r.next(type))
        {
            if (type >= kNumCmds)
            {
                break;
            }
            c.count[type]++;
            r.skip(payloadSizeOf(static_cast<RenderCmd>(type)));
        }
        return c;
    }
};

// 200x100, one filled rect, origin centered. Size is set after initialize():
// its layout hooks need the artboard resolved.
std::unique_ptr<Artboard> makeSource(Factory* factory,
                                     float width = 200.0f,
                                     float height = 100.0f,
                                     BitmapCache** cacheOut = nullptr)
{
    auto artboard = std::make_unique<Artboard>(factory);
    artboard->addObject(artboard.get());
    if (cacheOut != nullptr)
    {
        auto* cache = new BitmapCache();
        artboard->addObject(cache);
        cache->parentId(0);
        *cacheOut = cache;
    }

    auto* shape = new Shape();
    artboard->addObject(shape);
    shape->parentId(0);
    auto* rect = new Rectangle();
    artboard->addObject(rect);
    rect->parentId(artboard->idOf(shape));
    auto* fill = new Fill();
    artboard->addObject(fill);
    fill->parentId(artboard->idOf(shape));
    auto* color = new SolidColor();
    artboard->addObject(color);
    color->parentId(artboard->idOf(fill));
    REQUIRE(artboard->initialize() == StatusCode::Ok);
    artboard->width(width);
    artboard->height(height);
    artboard->originX(0.5f);
    artboard->originY(0.5f);
    rect->width(width);
    rect->height(height);
    artboard->advance(0.0f);
    return artboard;
}

std::vector<uint8_t> encodeIndices(const std::vector<uint16_t>& indices)
{
    std::vector<uint8_t> bytes;
    VectorBinaryWriter writer(&bytes);
    for (uint16_t index : indices)
    {
        writer.writeVarUint(static_cast<uint64_t>(index));
    }
    return bytes;
}

enum class HostDeformer
{
    mesh,
    nslice,
    none,
};

// Hosts `source` through a `hostType` NestedArtboard with a quad mesh or
// NSlicer.
struct MeshScene
{
    std::unique_ptr<gpu::RenderContext> renderContext =
        RenderContextNULL::MakeContext();
    CountingSession session{rive::ore::ReplayCaps{}};
    std::unique_ptr<Artboard> source;
    std::unique_ptr<Artboard> outer;
    NestedArtboard* nested = nullptr;
    Mesh* mesh = nullptr;
    NSlicer* slicer = nullptr;
    std::vector<MeshVertex*> vertices;
    StatusCode initStatus = StatusCode::Ok;

    ClippingShape* clip = nullptr;

    explicit MeshScene(HostDeformer deformer = HostDeformer::mesh,
                       uint16_t hostType = NestedArtboard::typeKey,
                       bool withHost = true,
                       bool clipped = false)
    {
        session.bindRenderContext(withHost ? renderContext.get() : nullptr);
        source = makeSource(&session);

        outer = std::make_unique<Artboard>(&session);
        outer->addObject(outer.get());

        nested = static_cast<NestedArtboard*>(
            CoreRegistry::makeCoreInstance(hostType));
        REQUIRE(nested != nullptr);
        outer->addObject(nested);
        nested->parentId(0);
        nested->x(250);
        nested->y(250);

        if (deformer == HostDeformer::mesh)
        {
            mesh = new Mesh();
            outer->addObject(mesh);
            mesh->parentId(outer->idOf(nested));
            // A quad over the source's box (origin centered), UVs 0..1.
            const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
            for (auto& corner : uv)
            {
                auto* v = new MeshVertex();
                v->u(corner[0]);
                v->v(corner[1]);
                outer->addObject(v);
                v->parentId(outer->idOf(mesh));
                vertices.push_back(v);
            }
            auto bytes = encodeIndices({0, 1, 2, 0, 2, 3});
            mesh->decodeTriangleIndexBytes(
                Span<const uint8_t>(bytes.data(), bytes.size()));
        }
        else if (deformer == HostDeformer::nslice)
        {
            slicer = new NSlicer();
            outer->addObject(slicer);
            slicer->parentId(outer->idOf(nested));
        }

        if (clipped)
        {
            // A small rect clips the instance to its top-left quarter.
            auto* clipShape = new Shape();
            outer->addObject(clipShape);
            clipShape->parentId(0);
            auto* clipRect = new Rectangle();
            outer->addObject(clipRect);
            clipRect->parentId(outer->idOf(clipShape));
            clipRect->width(50);
            clipRect->height(50);
            clip = new ClippingShape();
            outer->addObject(clip);
            clip->parentId(outer->idOf(nested));
            clip->sourceId(outer->idOf(clipShape));
        }

        initStatus = outer->initialize();
        if (initStatus == StatusCode::Ok)
        {
            outer->width(500);
            outer->height(500);
            // Position setters notify the mesh, so they wait for initialize().
            for (MeshVertex* v : vertices)
            {
                v->x((v->u() - 0.5f) * 200.0f);
                v->y((v->v() - 0.5f) * 100.0f);
            }
            nested->referencedArtboard(
                source->instance<ArtboardInstance>().release());
            outer->advance(0.0f);
        }
    }

    Census frame()
    {
        outer->advance(0.0f);
        Renderer* screen = session.screenRenderer();
        screen->save();
        outer->drawInternal(screen);
        screen->restore();
        snapshotFrame(session);
        Census c = Census::of(session.commandBuffer());
        session.resetFrame();
        return c;
    }
};
} // namespace

TEST_CASE("MeshHost accepts images and Node-mode instances only",
          "[artboard-mesh]")
{
    auto* node = new NestedArtboard();
    auto* leaf = new NestedArtboardLeaf();
    auto* layout = new NestedArtboardLayout();
    CHECK(MeshHost::from(node) != nullptr);
    CHECK(MeshHost::from(leaf) == nullptr);
    CHECK(MeshHost::from(layout) == nullptr);
    delete node;
    delete leaf;
    delete layout;
}

TEST_CASE("an instance without a mesh allocates no mesh state",
          "[artboard-mesh]")
{
    MeshScene scene(HostDeformer::none);
    REQUIRE(scene.initStatus == StatusCode::Ok);
    scene.frame();
    CHECK(scene.nested->meshHostIfAny() == nullptr);
}

TEST_CASE("a mesh under a Leaf or Layout instance is rejected",
          "[artboard-mesh]")
{
    for (uint16_t type :
         {NestedArtboardLeaf::typeKey, NestedArtboardLayout::typeKey})
    {
        MeshScene scene(HostDeformer::mesh, type);
        // MissingObject: the load succeeds without the mesh.
        CHECK(scene.nested->meshHostIfAny() == nullptr);
    }
}

TEST_CASE("a meshed instance draws its artboard through the mesh",
          "[artboard-mesh]")
{
    MeshScene scene;
    REQUIRE(scene.initStatus == StatusCode::Ok);
    REQUIRE(scene.nested->meshHost()->hostedMesh() == scene.mesh);

    Census c = scene.frame();
    CHECK(scene.session.rasters.size() == 1);
    CHECK(c.of(RenderCmd::canvasContentBegin) == 1);
    CHECK(c.of(RenderCmd::drawImageMesh) == 1);
    // The flat composite the plain cache would issue is not there.
    CHECK(c.of(RenderCmd::drawImage) == 0);
}

TEST_CASE("the raster is the artboard size times resolution", "[artboard-mesh]")
{
    MeshScene scene;
    scene.frame();
    REQUIRE(scene.session.rasters.size() == 1);
    CHECK(scene.session.rasters[0].width == 200);
    CHECK(scene.session.rasters[0].height == 100);
}

// Remounts the instance on a source with a BitmapCache; returns its copy.
BitmapCache* mountCachedSource(MeshScene& scene, float resolution, bool enabled)
{
    BitmapCache* sourceCache = nullptr;
    scene.source = makeSource(&scene.session, 200.0f, 100.0f, &sourceCache);
    sourceCache->resolution(resolution);
    CoreRegistry::setBool(sourceCache,
                          BitmapCacheBase::cacheEnabledPropertyKey,
                          enabled);
    scene.nested->referencedArtboard(
        scene.source->instance<ArtboardInstance>().release());
    scene.outer->advance(0.0f);
    BitmapCache* cache = scene.nested->artboardInstance()->bitmapCache();
    REQUIRE(cache != nullptr);
    REQUIRE(cache->resolution() == resolution);
    REQUIRE(cache->cacheEnabled() == enabled);
    return cache;
}

TEST_CASE("a Bitmap Cache resolution sets the mesh raster size",
          "[artboard-mesh]")
{
    MeshScene scene;
    mountCachedSource(scene, 2.0f, true);
    CHECK(scene.nested->meshHost()->meshRasterResolution() == 2.0f);

    scene.frame();
    REQUIRE(scene.session.rasters.size() == 1);
    CHECK(scene.session.rasters[0].width == 400);
    CHECK(scene.session.rasters[0].height == 200);
}

TEST_CASE("a meshed instance rasterizes with the cache switched off",
          "[artboard-mesh]")
{
    MeshScene scene;
    mountCachedSource(scene, 1.0f, false);
    Census c = scene.frame();
    CHECK(scene.session.rasters.size() == 1);
    CHECK(c.of(RenderCmd::drawImageMesh) == 1);
}

TEST_CASE("a static meshed instance reuses its raster", "[artboard-mesh]")
{
    MeshScene scene;
    scene.frame();
    for (int i = 0; i < 4; i++)
    {
        Census c = scene.frame();
        CHECK(c.of(RenderCmd::drawImageMesh) == 1);
    }
    CHECK(scene.session.rasters.size() == 1);
}

TEST_CASE("moving a vertex redraws without re-rasterizing", "[artboard-mesh]")
{
    MeshScene scene;
    scene.frame();
    scene.vertices[2]->x(scene.vertices[2]->x() + 40.0f);
    Census c = scene.frame();
    CHECK(c.of(RenderCmd::drawImageMesh) == 1);
    CHECK(scene.session.rasters.size() == 1);
}

TEST_CASE("swapping the instance's artboard re-rasterizes at the new size",
          "[artboard-mesh]")
{
    MeshScene scene;
    scene.frame();
    REQUIRE(scene.session.rasters.size() == 1);

    auto other = makeSource(&scene.session, 120.0f, 60.0f);
    // What a data-bound artboard swap does.
    scene.nested->referencedArtboard(
        other->instance<ArtboardInstance>().release());
    scene.outer->advance(0.0f);

    Census c = scene.frame();
    REQUIRE(scene.session.rasters.size() == 2);
    CHECK(scene.session.rasters[1].width == 120);
    CHECK(scene.session.rasters[1].height == 60);
    CHECK(c.of(RenderCmd::drawImageMesh) == 1);
    CHECK(scene.nested->meshHost()->hostedMesh() == scene.mesh);
}

TEST_CASE("UVs map onto the part of the raster the box covers",
          "[artboard-mesh]")
{
    // 200.5 wide rasterizes to 201 texels, so UV 1 lands at 200.5/201.
    MeshScene exact;
    CHECK(exact.nested->meshHost()->meshHostUVTransform() == Mat2D());

    MeshScene scene;
    auto other = makeSource(&scene.session, 200.5f, 100.0f);
    scene.nested->referencedArtboard(
        other->instance<ArtboardInstance>().release());
    const Mat2D uv = scene.nested->meshHost()->meshHostUVTransform();
    CHECK(uv.xx() == Approx(200.5f / 201.0f));
    CHECK(uv.yy() == Approx(1.0f));
}

TEST_CASE("the mesh host reports the instance's box", "[artboard-mesh]")
{
    MeshScene scene;
    CHECK(scene.nested->meshHost()->meshHostWidth() == 200.0f);
    CHECK(scene.nested->meshHost()->meshHostHeight() == 100.0f);
    const Vec2D origin = scene.nested->meshHost()->meshHostOrigin();
    CHECK(origin.x == Approx(0.5f));
    CHECK(origin.y == Approx(0.5f));
}

TEST_CASE("an instance without a canvas host draws vectors undeformed",
          "[artboard-mesh]")
{
    MeshScene scene(HostDeformer::mesh,
                    NestedArtboard::typeKey,
                    /*withHost=*/false);
    Census c = scene.frame();
    CHECK(scene.session.rasters.empty());
    CHECK(c.of(RenderCmd::drawImageMesh) == 0);
    CHECK(c.of(RenderCmd::drawPath) > 0);
}

TEST_CASE("a transparent meshed instance draws nothing", "[artboard-mesh]")
{
    MeshScene scene;
    scene.nested->opacity(0.0f);
    Census c = scene.frame();
    CHECK(c.of(RenderCmd::drawImageMesh) == 0);
    CHECK(scene.session.rasters.empty());

    // And it rasterizes once visible again.
    scene.nested->opacity(1.0f);
    c = scene.frame();
    CHECK(c.of(RenderCmd::drawImageMesh) == 1);
    CHECK(scene.session.rasters.size() == 1);
}

TEST_CASE("an opacity change re-rasterizes the instance", "[artboard-mesh]")
{
    // Opacity is baked into the raster via hostOpacity.
    MeshScene scene;
    scene.frame();
    scene.nested->opacity(0.5f);
    scene.frame();
    CHECK(scene.session.rasters.size() == 2);
}

TEST_CASE("an N-Slice under an instance draws through its slice mesh",
          "[artboard-mesh]")
{
    MeshScene scene(HostDeformer::nslice);
    REQUIRE(scene.initStatus == StatusCode::Ok);
    REQUIRE(scene.nested->meshHost()->hostedMesh() != nullptr);
    Census c = scene.frame();
    CHECK(scene.session.rasters.size() == 1);
    CHECK(c.of(RenderCmd::drawImageMesh) == 1);
}

TEST_CASE("a clipped meshed instance draws inside its clip", "[artboard-mesh]")
{
    MeshScene scene(HostDeformer::mesh,
                    NestedArtboard::typeKey,
                    /*withHost=*/true,
                    /*clipped=*/true);
    REQUIRE(scene.initStatus == StatusCode::Ok);
    Census c = scene.frame();
    CHECK(c.of(RenderCmd::drawImageMesh) == 1);
    // Frame clips appear either way; the ClippingShape adds one.
    MeshScene unclipped;
    CHECK(c.of(RenderCmd::clipPath) ==
          unclipped.frame().of(RenderCmd::clipPath) + 1);
}

TEST_CASE("a 2x display doubles the mesh raster", "[artboard-mesh]")
{
    struct ScaleScope
    {
        ~ScaleScope() { offscreen::setDisplayScale(1.0f); }
    } restore;
    offscreen::setDisplayScale(2.0f);

    MeshScene scene;
    scene.frame();
    REQUIRE(scene.session.rasters.size() == 1);
    CHECK(scene.session.rasters[0].width == 400);
    CHECK(scene.session.rasters[0].height == 200);
}

TEST_CASE("a bad display scale is ignored", "[artboard-mesh]")
{
    struct ScaleScope
    {
        ~ScaleScope() { offscreen::setDisplayScale(1.0f); }
    } restore;
    offscreen::setDisplayScale(2.0f);
    offscreen::setDisplayScale(0.0f);
    offscreen::setDisplayScale(std::numeric_limits<float>::quiet_NaN());
    CHECK(offscreen::displayScale() == 2.0f);
}

TEST_CASE("a draw visitor reaches the drawables of a meshed instance",
          "[artboard-mesh]")
{
    MeshScene scene;
    REQUIRE(scene.initStatus == StatusCode::Ok);
    scene.outer->advance(0.0f);
    int visited = 0;
    auto visitor = [](void* context, Drawable* drawable, Renderer* renderer) {
        ++*static_cast<int*>(context);
        drawable->draw(renderer);
    };
    Renderer* screen = scene.session.screenRenderer();
    screen->save();
    scene.outer->drawInternal(screen, visitor, &visited);
    screen->restore();
    snapshotFrame(scene.session);
    Census c = Census::of(scene.session.commandBuffer());
    scene.session.resetFrame();
    // Visits reach only drawables with custom properties; what matters is
    // the shape inside draws as vectors where a visit could reach it.
    CHECK(c.of(RenderCmd::drawImageMesh) == 0);
    CHECK(c.of(RenderCmd::drawPath) >= 1);

    // Without a visitor it deforms again.
    CHECK(scene.frame().of(RenderCmd::drawImageMesh) == 1);
}

TEST_CASE("a change drawn through the mesh refreshes the Bitmap Cache too",
          "[artboard-mesh]")
{
    MeshScene scene;
    REQUIRE(scene.initStatus == StatusCode::Ok);
    ArtboardInstance* instance = scene.nested->artboardInstance();
    REQUIRE(instance != nullptr);
    scene.frame();

    // The plain cache and the mesh raster are separate. A change consumed by
    // the mesh path must still be seen by the cache when the mesh goes away.
    offscreen::CachedRaster cache;
    offscreen::RasterPlan plan;
    cmd::DeferredCanvasHost* host = nullptr;
    Renderer* screen = scene.session.screenRenderer();
    REQUIRE(instance->rasterImage(screen, cache, 1.0f, true, &plan, &host) !=
            nullptr);
    const uint64_t before = cache.contentRevision;
    scene.session.resetFrame();

    instance->opacity(0.5f);
    scene.frame(); // the mesh raster consumes the change

    REQUIRE(instance->rasterImage(screen, cache, 1.0f, true, &plan, &host) !=
            nullptr);
    CHECK(cache.contentRevision != before);
    scene.session.resetFrame();
}

TEST_CASE("a clipped meshed instance draws vectors where the composite drops "
          "clips",
          "[artboard-mesh]")
{
    MeshScene scene(HostDeformer::mesh,
                    NestedArtboard::typeKey,
                    /*withHost=*/true,
                    /*clipped=*/true);
    REQUIRE(scene.initStatus == StatusCode::Ok);
    scene.session.dropsClips = true;
    Census c = scene.frame();
    CHECK(c.of(RenderCmd::drawImageMesh) == 0);
    CHECK(c.of(RenderCmd::drawPath) >= 1);

    // Unclipped instances keep deforming on such a host.
    MeshScene unclipped;
    unclipped.outer->clip(false);
    unclipped.session.dropsClips = true;
    CHECK(unclipped.frame().of(RenderCmd::drawImageMesh) == 1);
}

TEST_CASE("a meshed instance in a clipping artboard draws vectors where the "
          "composite drops clips",
          "[artboard-mesh]")
{
    MeshScene scene;
    REQUIRE(scene.initStatus == StatusCode::Ok);
    scene.session.dropsClips = true;
    scene.outer->clip(false);
    REQUIRE(scene.frame().of(RenderCmd::drawImageMesh) == 1);

    // The host artboard's frame clip is inherited, not a ClippingShape.
    scene.outer->clip(true);
    Census c = scene.frame();
    CHECK(c.of(RenderCmd::drawImageMesh) == 0);
    CHECK(c.of(RenderCmd::drawPath) >= 1);
}

TEST_CASE("an instance's own rotation tells its host the transform moved",
          "[artboard-mesh]")
{
    MeshScene scene;
    REQUIRE(scene.initStatus == StatusCode::Ok);
    scene.frame();
    ArtboardInstance* instance = scene.nested->artboardInstance();
    REQUIRE(instance != nullptr);
    REQUIRE(!scene.nested->hasDirt(ComponentDirt::Transform));

    instance->rotation(0.5f);
    CHECK(scene.nested->hasDirt(ComponentDirt::Transform));
}

static int s_transformDirtyCalls = 0;

TEST_CASE("every own-rotation frame notifies the editor host",
          "[artboard-mesh]")
{
    MeshScene scene;
    REQUIRE(scene.initStatus == StatusCode::Ok);
    scene.frame();
    ArtboardInstance* instance = scene.nested->artboardInstance();
    REQUIRE(instance != nullptr);
    s_transformDirtyCalls = 0;
    instance->callbackUserData = nullptr;
    instance->onTransformDirty([](void*) { s_transformDirtyCalls++; });
    instance->updatePass(false);

    // The editor advances a mount with advanceInternal() alone, and rotation
    // raises no component dirt, so no update pass runs between frames.
    for (int frame = 1; frame <= 3; frame++)
    {
        instance->rotation(0.1f * frame);
        instance->advanceInternal(0.0f, AdvanceFlags::None);
        CHECK(s_transformDirtyCalls == frame);
    }
}

TEST_CASE("a Component mesh host reports its source size before an instance "
          "exists",
          "[artboard-mesh]")
{
    MeshScene scene;
    REQUIRE(scene.initStatus == StatusCode::Ok);
    NestedArtboard probe;
    // nest() with a source (not an instance), as the editor does before
    // mounting, then triangulates.
    probe.referencedArtboard(scene.source.get());
    CHECK(probe.artboardInstance() == nullptr);
    CHECK(probe.meshHost()->meshHostWidth() == Approx(200.0f));
    CHECK(probe.meshHost()->meshHostHeight() == Approx(100.0f));
}

TEST_CASE("mounting a new instance releases the mesh raster at once",
          "[artboard-mesh]")
{
    MeshScene scene;
    REQUIRE(scene.initStatus == StatusCode::Ok);
    scene.frame();
    REQUIRE(scene.nested->meshHost()->rasterHeld());

    // Released at the mount, not on the next draw: draw-time pointer
    // equality misses a new instance that reuses a freed one's address.
    scene.nested->referencedArtboard(
        scene.source->instance<ArtboardInstance>().release());
    CHECK(!scene.nested->meshHost()->rasterHeld());
}

TEST_CASE("a clipped meshed instance with a cached source draws vectors where "
          "the composite drops clips",
          "[artboard-mesh]")
{
    MeshScene scene(HostDeformer::mesh,
                    NestedArtboard::typeKey,
                    /*withHost=*/true,
                    /*clipped=*/true);
    REQUIRE(scene.initStatus == StatusCode::Ok);
    mountCachedSource(scene, 1.0f, true);
    scene.session.dropsClips = true;
    Census c = scene.frame();
    // Neither the mesh nor the source's own cache composites.
    CHECK(c.of(RenderCmd::drawImageMesh) == 0);
    CHECK(c.of(RenderCmd::drawImage) == 0);
    CHECK(c.of(RenderCmd::drawPath) >= 1);
}

TEST_CASE("a change raised while the mesh rasterizes stays pending",
          "[artboard-mesh]")
{
    MeshScene scene;
    REQUIRE(scene.initStatus == StatusCode::Ok);
    scene.frame();
    ArtboardInstance* instance = scene.nested->artboardInstance();
    REQUIRE(instance != nullptr);
    Shape* shape = nullptr;
    for (Shape* s : instance->objects<Shape>())
    {
        shape = s;
        break;
    }
    REQUIRE(shape != nullptr);

    // Re-rasterize this frame, then move a node mid-raster, as a scripted
    // drawable's draw callback can.
    shape->x(shape->x() + 1.0f);
    scene.session.onContentDrawn = [&]() { shape->x(shape->x() + 1.0f); };
    scene.frame();
    scene.session.onContentDrawn = nullptr;
    REQUIRE(scene.session.rasters.size() == 2);
    CHECK(instance->didChange());
}
