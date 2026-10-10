#include "rive/nested_artboard_mesh_host.hpp"
#include "rive/artboard.hpp"
#include "rive/bitmap_cache.hpp"
#include "rive/layout_component.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/shapes/mesh_drawable.hpp"
#ifdef RIVE_CANVAS
#include "rive/renderer/cmd/deferred_canvas_host.hpp"
#endif

using namespace rive;

ArtboardInstance* NestedArtboardMeshHost::instance() const
{
    return m_owner->m_Instance.get();
}

void NestedArtboardMeshHost::resetRaster()
{
#ifdef RIVE_CANVAS
    // Revisions are per artboard, and a new instance can reuse a freed
    // one's address: never let a raster outlive the instance it drew.
    m_raster.release();
    m_rasterSource = nullptr;
    m_uvValid = false;
#endif
}

void NestedArtboardMeshHost::setMesh(MeshDrawable* mesh)
{
    if (m_mesh == mesh)
    {
        return;
    }
    m_mesh = mesh;
#ifdef RIVE_CANVAS
    m_uvValid = false;
    if (mesh == nullptr)
    {
        m_raster.release();
        m_rasterSource = nullptr;
    }
#endif
}

// The instance when mounted, else the resolved source: the editor
// triangulates right after nest(source), before any instance exists.
const Artboard* NestedArtboardMeshHost::sizeSource() const
{
    if (instance() != nullptr)
    {
        return instance();
    }
    return m_owner->m_referencedArtboard;
}

float NestedArtboardMeshHost::meshHostWidth() const
{
    const Artboard* a = sizeSource();
    return a != nullptr ? a->bounds().width() : 0.0f;
}

float NestedArtboardMeshHost::meshHostHeight() const
{
    const Artboard* a = sizeSource();
    return a != nullptr ? a->bounds().height() : 0.0f;
}

Vec2D NestedArtboardMeshHost::meshHostOrigin() const
{
    if (instance() == nullptr)
    {
        return Vec2D();
    }
    // frameOrigin is off when hosted, so bounds() starts at -size*origin.
    const AABB box = instance()->bounds();
    const float w = box.width();
    const float h = box.height();
    return Vec2D(w != 0.0f ? -box.left() / w : 0.0f,
                 h != 0.0f ? -box.top() / h : 0.0f);
}

Mat2D NestedArtboardMeshHost::meshHostWorldTransform() const
{
    // Matches the vector draw: host world, then the artboard's self transform.
    if (instance() == nullptr || !instance()->hasSelfTransform())
    {
        return m_owner->worldTransform();
    }
    return m_owner->worldTransform() * instance()->selfTransform();
}

float NestedArtboardMeshHost::meshRasterResolution() const
{
    if (instance() != nullptr && instance()->bitmapCache() != nullptr)
    {
        return instance()->bitmapCache()->resolution();
    }
    return 1.0f;
}

Mat2D NestedArtboardMeshHost::meshHostUVTransform() const
{
#ifdef RIVE_CANVAS
    if (instance() == nullptr)
    {
        return Mat2D();
    }
    // The raster is ceil()ed, so the box covers slightly under [0,1].
    offscreen::RasterPlan plan;
    if (!offscreen::planFixedRasterSize(instance()->bounds(),
                                        meshRasterResolution(),
                                        &plan))
    {
        return Mat2D();
    }
    return Mat2D::fromScale(plan.box.width() * plan.rasterScale /
                                static_cast<float>(plan.widthPx),
                            plan.box.height() * plan.rasterScale /
                                static_cast<float>(plan.heightPx));
#else
    return Mat2D();
#endif
}

#ifdef RIVE_CANVAS
// Any clip in force where this draws: its own clipping shapes, a clipping
// layout or artboard above it, and the same for every host above that.
bool NestedArtboardMeshHost::underClip() const
{
    for (const Component* c = m_owner; c != nullptr;)
    {
        if (c->is<Drawable>() && !c->as<Drawable>()->clippingShapes().empty())
        {
            return true;
        }
        if (c != m_owner && c->is<LayoutComponent>() &&
            c->as<LayoutComponent>()->clip())
        {
            return true;
        }
        const ContainerComponent* parent = c->parent();
        if (parent != nullptr)
        {
            c = parent;
            continue;
        }
        // The artboard root: continue from whatever hosts it.
        const Artboard* ab = c->is<Artboard>() ? c->as<Artboard>() : nullptr;
        ArtboardHost* host = ab != nullptr ? ab->host() : nullptr;
        c = host != nullptr ? host->hostComponent() : nullptr;
    }
    return false;
}

// A fresh composite renderer (WebGL) inherits no clip.
bool NestedArtboardMeshHost::compositeDropsClip() const
{
    if (instance() == nullptr || !underClip())
    {
        return false;
    }
    Factory* f = instance()->factory();
    cmd::DeferredCanvasHost* host = f ? f->canvasContentHost() : nullptr;
    return host != nullptr && host->compositeDropsClips();
}

bool NestedArtboardMeshHost::draw(Renderer* renderer)
{
    ArtboardInstance* instance = this->instance();
    if (instance == nullptr)
    {
        return true;
    }
    if (m_rasterSource != instance)
    {
        // Data-bound swap: the old raster and UVs don't apply.
        m_raster.release();
        m_rasterSource = instance;
        m_uvValid = false;
    }
    if (instance->consumeInvisibleChange())
    {
        return true;
    }

    // Ignores Bitmap Cache Enabled: a deformed instance needs a bitmap.
    offscreen::RasterPlan plan;
    cmd::DeferredCanvasHost* host = nullptr;
    rcp<RenderImage> image = instance->rasterImage(renderer,
                                                   m_raster,
                                                   meshRasterResolution(),
                                                   /*fixedSize=*/true,
                                                   &plan,
                                                   &host);
    if (image == nullptr)
    {
        return false;
    }

    const Mat2D uvTransform = meshHostUVTransform();
    if (!m_uvValid || !(uvTransform == m_uvTransform))
    {
        m_uvTransform = uvTransform;
        m_uvValid = true;
        m_mesh->uvTransformChanged(uvTransform);
    }

    // Opacity is in the raster; only an enclosing modulateOpacity() remains.
    auto placement = offscreen::beginPlacement(renderer, host, plan);
    m_mesh->draw(placement.renderer,
                 image.get(),
                 ImageSampler::LinearClamp(),
                 m_owner->blendMode(),
                 placement.opacity,
                 0.0f);
    offscreen::endComposite(placement);
    return true;
}
#endif

float NestedArtboardMeshHost::meshHostScaleX() const
{
    return m_owner->scaleX();
}

float NestedArtboardMeshHost::meshHostScaleY() const
{
    return m_owner->scaleY();
}

bool NestedArtboardMeshHost::meshHostReady() const
{
    return instance() != nullptr;
}
