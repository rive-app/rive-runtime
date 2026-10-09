#include <rive/artboard.hpp>
#include <rive/bones/root_bone.hpp>
#include <rive/bones/skin.hpp>
#include <rive/bones/tendon.hpp>
#include <rive/bones/weight.hpp>
#include <rive/shapes/cubic_detached_vertex.hpp>
#include <rive/shapes/paint/fill.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/shapes/points_path.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/shapes/straight_vertex.hpp>
#include <utils/no_op_factory.hpp>
#include <catch.hpp>

namespace
{
// A path bound to one bone: two straight vertices weighted to it, and a cubic
// vertex given [cubicWeight] (which may be null) instead of a CubicWeight.
struct CubicRig
{
    rive::NoOpFactory factory;
    rive::Artboard artboard{&factory};
    rive::RootBone* bone = new rive::RootBone();
    rive::StraightVertex* weighted[2];
    rive::CubicDetachedVertex* cubic = new rive::CubicDetachedVertex();

    CubicRig(rive::Weight* cubicWeight)
    {
        artboard.addObject(&artboard);
        auto add = [&](rive::Component* component, rive::Core* parent) {
            artboard.addObject(component);
            component->parentId(artboard.idOf(parent));
        };

        add(bone, &artboard);
        auto shape = new rive::Shape();
        add(shape, &artboard);
        auto fill = new rive::Fill();
        add(fill, shape);
        add(new rive::SolidColor(), fill);
        auto path = new rive::PointsPath();
        path->isClosed(true);
        add(path, shape);

        for (int i = 0; i < 2; i++)
        {
            auto vertex = new rive::StraightVertex();
            vertex->x(i * 100.0f);
            add(vertex, path);
            auto weight = new rive::Weight();
            weight->values(255);
            weight->indices(1);
            add(weight, vertex);
            weighted[i] = vertex;
        }
        cubic->x(50.0f);
        cubic->y(100.0f);
        add(cubic, path);
        if (cubicWeight != nullptr)
        {
            add(cubicWeight, cubic);
        }

        auto skin = new rive::Skin();
        add(skin, path);
        auto tendon = new rive::Tendon();
        tendon->boneId(artboard.idOf(bone));
        add(tendon, skin);

        REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    }

    // Deforms with the rig live: the weighted vertices follow the bone, the
    // unweighted cubic one holds its bind position.
    void checkDeform()
    {
        artboard.advance(0.0f);
        bone->x(10.0f);
        artboard.advance(0.0f);
        CHECK(weighted[0]->renderTranslation() == rive::Vec2D(10.0f, 0.0f));
        CHECK(weighted[1]->renderTranslation() == rive::Vec2D(110.0f, 0.0f));
        CHECK(cubic->renderTranslation() == rive::Vec2D(50.0f, 100.0f));
    }
};
} // namespace

// Sentry RIVE_NATIVE-145: a skinned path whose cubic vertex has no
// CubicWeight (one the agent's add_vertices tool appended after the bind)
// crashed CubicVertex::deform in runtime builds.
TEST_CASE("a skinned cubic vertex without a weight does not crash the deform",
          "[skin]")
{
    CubicRig rig(nullptr);
    REQUIRE(!rig.cubic->hasWeight());
    rig.checkDeform();
}

// A base Weight under a cubic vertex would be read as a CubicWeight, past
// the end of the object, so the vertex is left unweighted instead.
TEST_CASE("a base weight under a skinned cubic vertex is not used", "[skin]")
{
    auto weight = new rive::Weight();
    weight->values(255);
    weight->indices(1);
    CubicRig rig(weight);
    REQUIRE(!rig.cubic->hasWeight());
    rig.checkDeform();
}
