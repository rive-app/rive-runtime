#include <rive/artboard.hpp>
#include <rive/bones/bone.hpp>
#include <rive/bones/cubic_weight.hpp>
#include <rive/bones/root_bone.hpp>
#include <rive/bones/skin.hpp>
#include <rive/bones/tendon.hpp>
#include <rive/bones/weight.hpp>
#include <rive/file.hpp>
#include <rive/math/math_types.hpp>
#include <rive/shapes/cubic_detached_vertex.hpp>
#include <rive/shapes/paint/fill.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/shapes/points_path.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/shapes/shape_path_flags.hpp>
#include <rive/shapes/straight_vertex.hpp>
#include <utils/no_op_factory.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cmath>

namespace
{
// A [size] wide square quad with a clockwise fill, its top edge bound to one
// root bone and its bottom edge to another. A lens spans the same box with two
// curved points on the bottom bone, so only its handles, one pair per bone,
// give it area.
struct QuadRig
{
    rive::NoOpFactory factory;
    rive::Artboard artboard{&factory};
    rive::RootBone* top = new rive::RootBone();
    rive::RootBone* bottom = new rive::RootBone();
    rive::Shape* shape = new rive::Shape();
    rive::PointsPath* path = new rive::PointsPath();
    rive::Skin* skin = new rive::Skin();
    std::vector<rive::PathVertex*> vertices;

    // The top bone is bound at y = 0 but starts at [topStartY], so a value
    // past the bottom bone loads the path folded inside out.
    QuadRig(bool clockwise,
            float topStartY = 0.0f,
            bool lens = false,
            float size = 100.0f)
    {
        artboard.addObject(&artboard);
        auto add = [&](rive::Component* component, rive::Core* parent) {
            artboard.addObject(component);
            component->parentId(artboard.idOf(parent));
        };
        add(top, &artboard);
        bottom->y(size);
        add(bottom, &artboard);
        add(shape, &artboard);
        auto fill = new rive::Fill();
        fill->fillRule((uint32_t)rive::FillRule::clockwise);
        add(fill, shape);
        add(new rive::SolidColor(), fill);
        path->isClosed(true);
        if (!clockwise)
        {
            path->pathFlags((uint32_t)rive::ShapePathFlags::isCounterClockwise);
        }
        add(path, shape);

        // The first point's out handle and the second's in handle sit on the
        // top edge, the other two on the bottom edge.
        for (int i = 0; lens && i < 2; i++)
        {
            auto vertex = new rive::CubicDetachedVertex();
            vertex->x((i == 0) == clockwise ? 0.0f : size);
            vertex->y(size / 2);
            vertex->outRotation((i == 0 ? -0.5f : 0.5f) * rive::math::PI);
            vertex->inRotation((i == 0 ? 0.5f : -0.5f) * rive::math::PI);
            vertex->outDistance(size / 2);
            vertex->inDistance(size / 2);
            add(vertex, path);
            auto weight = new rive::CubicWeight();
            weight->values(255);
            weight->indices(2);
            weight->outValues(255);
            weight->outIndices(i == 0 ? 1 : 2);
            weight->inValues(255);
            weight->inIndices(i == 0 ? 2 : 1);
            add(weight, vertex);
            vertices.push_back(vertex);
        }

        float corners[4][2] = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
        for (int i = 0; !lens && i < 4; i++)
        {
            auto corner = corners[clockwise ? i : 3 - i];
            auto vertex = new rive::StraightVertex();
            vertex->x(corner[0] * size / 100);
            vertex->y(corner[1] * size / 100);
            add(vertex, path);
            auto weight = new rive::Weight();
            weight->values(255);
            // Tendon slots start at 1, the top bone's tendon comes first.
            weight->indices(corner[1] == 0 ? 1 : 2);
            add(weight, vertex);
            vertices.push_back(vertex);
        }

        add(skin, path);
        for (auto bone : {top, bottom})
        {
            auto tendon = new rive::Tendon();
            tendon->boneId(artboard.idOf(bone));
            tendon->ty(bone->y());
            add(tendon, skin);
        }
        top->y(topStartY);
        REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    }

    void scale(rive::RootBone* bone, float x, float y)
    {
        bone->scaleX(x);
        bone->scaleY(y);
    }

    // Area of the path as the clockwise fill receives it, positive when it
    // really is clockwise.
    float composedArea()
    {
        artboard.advance(0.0f);
        return shape->localClockwisePath()->rawPath()->computeCoarseArea();
    }

    float deformedArea()
    {
        auto& rawPath = path->rawPath();
        return rawPath.computeCoarseArea(rawPath.bounds().center());
    }
};

bool isAncestor(rive::Component* ancestor, rive::Component* of)
{
    for (auto c = of->parent(); c != nullptr; c = c->parent())
    {
        if (c == ancestor)
        {
            return true;
        }
    }
    return false;
}

// Lowest bone that every tendon of the skin hangs under.
rive::Bone* commonBone(rive::Skin* skin)
{
    auto& tendons = skin->tendons();
    for (rive::Component* c = tendons[0]->bone(); c != nullptr; c = c->parent())
    {
        if (!c->is<rive::Bone>())
        {
            continue;
        }
        bool coversAll = true;
        for (auto tendon : tendons)
        {
            if (tendon->bone() != c && !isAncestor(c, tendon->bone()))
            {
                coversAll = false;
                break;
            }
        }
        if (coversAll)
        {
            return c->as<rive::Bone>();
        }
    }
    return nullptr;
}

rive::Fill* firstFill(rive::Shape* shape)
{
    for (auto paint : shape->shapePaints())
    {
        if (paint->is<rive::Fill>())
        {
            return paint->as<rive::Fill>();
        }
    }
    return nullptr;
}
} // namespace

TEST_CASE("unskinned and resting skinned quads compose clockwise",
          "[skinwinding]")
{
    for (bool clockwise : {true, false})
    {
        QuadRig rig(clockwise);
        CHECK(rig.composedArea() > 0);
        CHECK(rig.skin->windingSign() == 1);
        CHECK((rig.deformedArea() > 0) == clockwise);
    }
}

TEST_CASE("a collapsed first frame is not cached as the winding",
          "[skinwinding]")
{
    QuadRig rig(false);
    rig.scale(rig.top, 0.0f, 0.0f);
    rig.scale(rig.bottom, 0.0f, 0.0f);
    rig.artboard.advance(0.0f);
    CHECK(rig.skin->windingSign() == 0);
    CHECK(rig.deformedArea() == 0.0f);

    rig.scale(rig.top, 1.0f, 1.0f);
    rig.scale(rig.bottom, 1.0f, 1.0f);
    CHECK(rig.composedArea() > 0);
    CHECK(rig.deformedArea() < 0);
}

TEST_CASE("a collapsed bone does not vote on the mirroring", "[skinwinding]")
{
    QuadRig rig(true);
    CHECK(rig.composedArea() > 0);

    rig.scale(rig.top, -1.0f, 1.0f);
    rig.scale(rig.bottom, 0.0f, 0.0f);
    CHECK(rig.composedArea() > 0);
    CHECK(rig.skin->windingSign() == -1);
    CHECK(rig.deformedArea() < 0);

    rig.scale(rig.bottom, -1.0f, 1.0f);
    CHECK(rig.composedArea() > 0);
    CHECK(rig.skin->windingSign() == -1);
}

TEST_CASE("a mixed first frame leaves nothing wrong cached", "[skinwinding]")
{
    QuadRig rig(false);
    rig.scale(rig.top, -1.5f, 1.0f);
    CHECK(rig.composedArea() > 0);
    CHECK(rig.skin->windingSign() == 0);
    // The half mirrored quad winds the other way from its resting self.
    CHECK(rig.deformedArea() > 0);

    rig.scale(rig.top, 1.0f, 1.0f);
    CHECK(rig.composedArea() > 0);
    CHECK(rig.skin->windingSign() == 1);
    CHECK(rig.deformedArea() < 0);

    rig.scale(rig.top, -1.0f, 1.0f);
    rig.scale(rig.bottom, -1.0f, 1.0f);
    CHECK(rig.composedArea() > 0);
    CHECK(rig.skin->windingSign() == -1);
    CHECK(rig.deformedArea() > 0);
}

TEST_CASE("a rig far from the origin still measures its winding",
          "[skinwinding]")
{
    QuadRig rig(false);
    for (auto bone : {rig.top, rig.bottom})
    {
        bone->x(1e5f);
        bone->y(bone->y() + 1e5f);
    }
    CHECK(rig.composedArea() > 0);
    CHECK(rig.deformedArea() == Approx(-10000.0f));

    // Only a cached measurement keeps this right, the authored flag is stale.
    rig.scale(rig.top, -1.0f, 1.0f);
    rig.scale(rig.bottom, -1.0f, 1.0f);
    CHECK(rig.composedArea() > 0);
    CHECK(rig.deformedArea() == Approx(10000.0f));
}

TEST_CASE("moving a vertex measures the winding again", "[skinwinding]")
{
    QuadRig rig(true);
    CHECK(rig.composedArea() > 0);

    for (auto vertex : rig.vertices)
    {
        vertex->x(-vertex->x());
    }
    CHECK(rig.composedArea() > 0);
    CHECK(rig.deformedArea() < 0);
}

// Pins a known limitation so that changing it is deliberate: bones that fold
// the path inside out without mirroring keep the winding it was bound with.
TEST_CASE("a fold without mirroring keeps the bound winding", "[skinwinding]")
{
    QuadRig rig(true);
    CHECK(rig.composedArea() > 0);

    rig.top->y(200.0f);
    float composed = rig.composedArea();
    CHECK(rig.skin->windingSign() == 1);
    CHECK(rig.deformedArea() < 0);
    CHECK(composed < 0);
}

// The winding is taken from the path as bound, not from whatever pose the first
// frame shows: a rig that loads folded (say a data bound size that starts
// narrow) must still fill once its bones unfold it.
TEST_CASE("a folded first frame is not cached as the winding", "[skinwinding]")
{
    for (bool clockwise : {true, false})
    {
        QuadRig rig(clockwise, 200.0f);
        float folded = rig.composedArea();
        CHECK(rig.skin->windingSign() == 1);
        CHECK((rig.deformedArea() < 0) == clockwise);
        // Folded without mirroring, as in the test above.
        CHECK(folded < 0);

        rig.top->y(0.0f);
        CHECK(rig.composedArea() > 0);
        CHECK((rig.deformedArea() > 0) == clockwise);
    }
}

// Its straight edges would enclose nothing, so a bind pose that dropped the
// handles would fall back to caching the folded pose.
TEST_CASE("a curved path takes its bound winding from its handles",
          "[skinwinding]")
{
    for (bool clockwise : {true, false})
    {
        QuadRig rig(clockwise, 200.0f, true);
        CHECK(rig.composedArea() < 0);
        CHECK((rig.deformedArea() < 0) == clockwise);

        rig.top->y(0.0f);
        CHECK(rig.composedArea() > 0);
        CHECK((rig.deformedArea() > 0) == clockwise);
    }
}

// Small enough that a measure linearizing curves by pixel tolerance would only
// see the chords, which enclose nothing.
TEST_CASE("a small curved path follows mirroring from its handles",
          "[skinwinding]")
{
    for (bool clockwise : {true, false})
    {
        QuadRig rig(clockwise, 0.0f, true, 8.0f);
        int authored = clockwise ? 1 : -1;
        rig.artboard.advance(0.0f);
        CHECK(rig.path->winding() == authored);

        rig.scale(rig.top, -1.0f, 1.0f);
        rig.scale(rig.bottom, -1.0f, 1.0f);
        rig.artboard.advance(0.0f);
        CHECK(rig.skin->windingSign() == -1);
        CHECK(rig.path->winding() == -authored);
    }
}

// Folding the lens and mirroring only its top bone leaves the pose to be
// measured, at a size where a pixel tolerance would only see the chords.
TEST_CASE("a small curved path measures its pose from its handles",
          "[skinwinding]")
{
    for (bool clockwise : {true, false})
    {
        QuadRig rig(clockwise, 8.0f, true, 4.0f);
        rig.scale(rig.top, 1.0f, -1.0f);
        rig.artboard.advance(0.0f);
        CHECK(rig.skin->windingSign() == 0);
        CHECK(rig.path->winding() == (clockwise ? -1 : 1));
    }
}

TEST_CASE("a real rig stays clockwise when its bones mirror", "[skinwinding]")
{
    auto file = ReadRiveFile("assets/zombie_skins.riv");
    auto source = file->artboard();

    // A filled single path shape bound to bones, hung off the artboard so the
    // bones can mirror without the shape following.
    rive::Shape* sourceShape = nullptr;
    rive::Bone* sourceBone = nullptr;
    for (auto core : source->objects())
    {
        if (core == nullptr || !core->is<rive::PointsPath>())
        {
            continue;
        }
        auto path = core->as<rive::PointsPath>();
        auto shape = path->shape();
        if (path->skin() == nullptr || shape->paths().size() != 1 ||
            firstFill(shape) == nullptr || path->skin()->tendons().size() < 2)
        {
            continue;
        }
        sourceBone = commonBone(path->skin());
        if (sourceBone != nullptr)
        {
            sourceShape = shape;
            break;
        }
    }
    REQUIRE(sourceShape != nullptr);
    sourceShape->parentId(0);

    auto artboard = source->instance();
    auto shape =
        artboard->objects()[source->idOf(sourceShape)]->as<rive::Shape>();
    auto bone = artboard->objects()[source->idOf(sourceBone)]->as<rive::Bone>();
    REQUIRE(shape->parent() == artboard.get());
    REQUIRE(!isAncestor(bone, shape));
    auto path = shape->paths()[0]->as<rive::PointsPath>();
    auto skin = path->skin();

    firstFill(shape)->fillRule((uint32_t)rive::FillRule::clockwise);
    shape->pathChanged();
    artboard->advance(0.0f);
    float restArea = path->rawPath().computeCoarseArea();
    CHECK(shape->localClockwisePath()->rawPath()->computeCoarseArea() > 0);
    CHECK(skin->windingSign() == 1);

    bone->scaleX(-1.0f);
    artboard->advance(0.0f);
    CHECK(skin->windingSign() == -1);
    CHECK(path->rawPath().computeCoarseArea() * restArea < 0);
    CHECK(shape->localClockwisePath()->rawPath()->computeCoarseArea() > 0);
    bone->scaleX(1.0f);

    // Mirror the first bone no other tendon hangs under, folding the path.
    rive::Bone* leaf = nullptr;
    for (auto tendon : skin->tendons())
    {
        bool hasChildTendon = false;
        for (auto other : skin->tendons())
        {
            hasChildTendon |= isAncestor(tendon->bone(), other->bone());
        }
        if (!hasChildTendon && tendon->bone() != bone)
        {
            leaf = tendon->bone();
            break;
        }
    }
    REQUIRE(leaf != nullptr);
    leaf->scaleX(-1.0f);
    artboard->advance(0.0f);
    CHECK(skin->windingSign() == 0);
    // The fold has to have moved the path for this to test anything.
    float foldedArea = path->rawPath().computeCoarseArea();
    CHECK(std::abs(foldedArea - restArea) > 0.01f * std::abs(restArea));
    CHECK(shape->localClockwisePath()->rawPath()->computeCoarseArea() *
              foldedArea >=
          0);
}
