#include <rive/artboard.hpp>
#include <rive/bones/bone.hpp>
#include <rive/bones/root_bone.hpp>
#include <rive/bones/skin.hpp>
#include <rive/bones/tendon.hpp>
#include <rive/bones/weight.hpp>
#include <rive/file.hpp>
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
// A 100x100 quad with a clockwise fill, its top edge bound to one root bone
// and its bottom edge to another.
struct QuadRig
{
    rive::NoOpFactory factory;
    rive::Artboard artboard{&factory};
    rive::RootBone* top = new rive::RootBone();
    rive::RootBone* bottom = new rive::RootBone();
    rive::Shape* shape = new rive::Shape();
    rive::PointsPath* path = new rive::PointsPath();
    rive::Skin* skin = new rive::Skin();
    std::vector<rive::StraightVertex*> vertices;

    QuadRig(bool clockwise)
    {
        artboard.addObject(&artboard);
        auto add = [&](rive::Component* component, rive::Core* parent) {
            artboard.addObject(component);
            component->parentId(artboard.idOf(parent));
        };
        add(top, &artboard);
        bottom->y(100.0f);
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

        float corners[4][2] = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
        for (int i = 0; i < 4; i++)
        {
            auto corner = corners[clockwise ? i : 3 - i];
            auto vertex = new rive::StraightVertex();
            vertex->x(corner[0]);
            vertex->y(corner[1]);
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
// the path inside out without mirroring keep the winding measured before.
TEST_CASE("a fold without mirroring keeps the measured winding",
          "[skinwinding]")
{
    QuadRig rig(true);
    CHECK(rig.composedArea() > 0);

    rig.top->y(200.0f);
    float composed = rig.composedArea();
    CHECK(rig.skin->windingSign() == 1);
    CHECK(rig.deformedArea() < 0);
    CHECK(composed < 0);
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
