/*
 * Copyright 2026 Rive
 */

// PathComposer keeps a shape's local path when nothing it is built from has
// changed. Its inputs are each path's geometry and each path's transform
// relative to the shape (inverseWorld * pathTransform), and that relative
// transform survives any rigid move of the shape -- an ancestor translating,
// rotating or scaling moves the shape and its paths together, so the local
// path it would rebuild is the one it already holds. Skipping that rebuild is
// what lets the RenderPath keep its cached triangulation.
//
// The contract this pins down is that the retained path stays equal to a fresh
// rebuild. It cannot be bit equality: the composed transform is an A^-1 * A
// round trip, so recomputing it after a move lands a rounding step away from
// the one the buffer was built with. So: the verb stream must match exactly,
// and points must agree to within a few ULPs of their own magnitude.

#include "rive/file.hpp"
#include "rive/math/math_types.hpp"
#include "rive/shapes/path.hpp"
#include "rive/shapes/path_composer.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <cmath>
#include <limits>
#include <vector>

using namespace rive;

namespace
{
// Real files, chosen to cover what feeds the composer: plain fills, skins,
// solos, clips, trim paths and follow paths.
const char* kFiles[] = {
    "assets/car_widgets_v01.riv",
    "assets/zombie_skins.riv",
    "assets/echo_show_demo.riv",
    "assets/jellyfish_test.riv",
    "assets/trim_path.riv",
    "assets/fill_trim_path.riv",
    "assets/follow_path.riv",
    "assets/follow_path_shapes.riv",
    "assets/follow_path_solos.riv",
    "assets/solo_test.riv",
    "assets/clip_tests.riv",
    "assets/clipping_and_draw_order.riv",
};

constexpr int kFrames = 120;

// Force every path to rebuild its geometry, which is an input the composer
// cannot skip on, and settle the artboard. Whatever the composer holds
// afterwards is a fresh build by its own code rather than a reimplementation
// of it here.
void forceRebuild(Artboard* artboard)
{
    for (auto shape : artboard->find<Shape>())
    {
        for (auto path : shape->paths())
        {
            path->markPathDirty();
        }
    }
    artboard->advance(0.0f);
}

// How far a point is allowed to sit from a freshly rebuilt one. The error is
// the rounding of recomposing inverseWorld * pathTransform, so it scales with
// the world coordinates that go through that cancellation -- not with the
// local coordinate it lands on.
double allowedDelta(Shape* shape)
{
    const Mat2D& world = shape->worldTransform();
    double scale =
        std::max({1.0, (double)std::abs(world[4]), (double)std::abs(world[5])});
    for (auto path : shape->paths())
    {
        const Mat2D& pathWorld = path->pathTransform();
        scale = std::max({scale,
                          (double)std::abs(pathWorld[4]),
                          (double)std::abs(pathWorld[5])});
    }
    return 16.0 * std::numeric_limits<float>::epsilon() * scale;
}

// Drive one frame: animate, and move the whole artboard rigidly so every
// shape's world transform changes without any of them deforming.
void advanceFrame(Artboard* artboard, StateMachineInstance* machine, int i)
{
    artboard->mutableWorldTransform() =
        Mat2D::fromRotation((float)i * 0.013f) *
        Mat2D::fromTranslate((float)(i % 23) * 1.7f, (float)(i % 7));
    artboard->markWorldTransformDirty();
    if (machine != nullptr)
    {
        machine->pointerMove(
            Vec2D((float)(i * 13 % 500), (float)(i * 29 % 500)));
        machine->advanceAndApply(1.0f / 60.0f);
    }
    else
    {
        artboard->advance(1.0f / 60.0f);
    }
}

// Shapes whose path update is deferred (transparent and not clipping) hold a
// stale path by design, and n-sliced shapes are fed by their slicer rather
// than by the loop above.
bool isComparable(Shape* shape)
{
    return shape->isFlagged(PathFlags::local) && !shape->canDeferPathUpdate() &&
           !shape->isFlagged(PathFlags::followPath);
}
} // namespace

TEST_CASE("a retained local path equals a fresh rebuild", "[path_composer]")
{
    for (auto name : kFiles)
    {
        auto file = ReadRiveFile(name);
        auto artboard = file->artboard()->instance();
        auto machine = artboard->defaultStateMachine();
        artboard->advance(0.0f);

        size_t verbMismatches = 0;
        size_t violations = 0;
        double worstDelta = 0.0;
        double worstAllowed = 0.0;

        for (int i = 0; i < kFrames; i++)
        {
            advanceFrame(artboard.get(), machine.get(), i);

            // What the composer is handing the renderer this frame.
            std::vector<Shape*> shapes;
            std::vector<RawPath> held;
            for (auto shape : artboard->find<Shape>())
            {
                if (isComparable(shape))
                {
                    shapes.push_back(shape);
                    held.push_back(
                        *shape->pathComposer()->localPath()->rawPath());
                }
            }

            forceRebuild(artboard.get());

            for (size_t s = 0; s < shapes.size(); s++)
            {
                const RawPath& fresh =
                    *shapes[s]->pathComposer()->localPath()->rawPath();
                const RawPath& kept = held[s];
                if (kept.verbs().count() != fresh.verbs().count() ||
                    kept.points().count() != fresh.points().count())
                {
                    verbMismatches++;
                    continue;
                }
                for (size_t v = 0; v < kept.verbs().count(); v++)
                {
                    if (kept.verbs()[v] != fresh.verbs()[v])
                    {
                        verbMismatches++;
                        break;
                    }
                }
                const double allowed = allowedDelta(shapes[s]);
                for (size_t p = 0; p < kept.points().count(); p++)
                {
                    const Vec2D a = kept.points()[p];
                    const Vec2D b = fresh.points()[p];
                    const double delta =
                        std::max(std::abs((double)a.x - (double)b.x),
                                 std::abs((double)a.y - (double)b.y));
                    if (delta > allowed)
                    {
                        violations++;
                    }
                    if (delta > worstDelta)
                    {
                        worstDelta = delta;
                        worstAllowed = allowed;
                    }
                }
            }
        }

        INFO("file " << name << ", worst point delta " << worstDelta
                     << " against an allowance of " << worstAllowed);
        CHECK(verbMismatches == 0);
        CHECK(violations == 0);
    }
}

// The above passes trivially if the composer rebuilds every frame, so pin down
// that it actually skips: a rigidly moved shape keeps the exact points it was
// built with, which a rebuild would have replaced with recomposed ones.
TEST_CASE("a rigid move retains the local path it already built",
          "[path_composer]")
{
    auto file = ReadRiveFile("assets/car_widgets_v01.riv");
    auto artboard = file->artboard()->instance();
    artboard->advance(0.0f);

    Shape* moved = nullptr;
    std::vector<Vec2D> before;
    for (auto shape : artboard->find<Shape>())
    {
        if (isComparable(shape) &&
            !shape->pathComposer()->localPath()->rawPath()->empty())
        {
            moved = shape;
            auto points =
                shape->pathComposer()->localPath()->rawPath()->points();
            before.assign(points.begin(), points.end());
            break;
        }
    }
    REQUIRE(moved != nullptr);
    REQUIRE(!before.empty());

    // Rotate, scale and translate the artboard. None of that changes any
    // path's transform relative to its shape.
    artboard->mutableWorldTransform() = Mat2D::fromRotation(math::PI / 3.0f) *
                                        Mat2D::fromScale(2.0f, 0.5f) *
                                        Mat2D::fromTranslate(137.0f, -42.0f);
    artboard->markWorldTransformDirty();
    artboard->advance(0.0f);

    auto after = moved->pathComposer()->localPath()->rawPath()->points();
    REQUIRE(after.count() == before.size());
    for (size_t i = 0; i < before.size(); i++)
    {
        // Bit-identical: the buffer was never touched.
        CHECK(after[i].x == before[i].x);
        CHECK(after[i].y == before[i].y);
    }
}

// The dangerous case for a skip: a path moves relative to its shape without
// its geometry changing. Nothing rebuilds its raw path -- only the transform
// composed into the local path changes -- so a skip that watched geometry
// alone would keep drawing the path where it used to be.
TEST_CASE("a path moving inside its shape rebuilds the local path",
          "[path_composer]")
{
    auto file = ReadRiveFile("assets/car_widgets_v01.riv");
    auto artboard = file->artboard()->instance();
    artboard->advance(0.0f);

    Shape* shape = nullptr;
    for (auto candidate : artboard->find<Shape>())
    {
        if (isComparable(candidate) && !candidate->paths().empty() &&
            !candidate->pathComposer()->localPath()->rawPath()->empty())
        {
            shape = candidate;
            break;
        }
    }
    REQUIRE(shape != nullptr);
    auto path = shape->paths()[0];

    auto points = shape->pathComposer()->localPath()->rawPath()->points();
    std::vector<Vec2D> before(points.begin(), points.end());
    const uint32_t geometryVersion = path->geometryVersion();

    // Move the path within the shape. This is a transform change, so the
    // path's own geometry is untouched...
    path->x(path->x() + 10.0f);
    path->markTransformDirty();
    artboard->advance(0.0f);
    CHECK(path->geometryVersion() == geometryVersion);

    // ...but the shape's local path has to follow it.
    auto moved = shape->pathComposer()->localPath()->rawPath()->points();
    REQUIRE(moved.count() == before.size());
    bool anyMoved = false;
    for (size_t i = 0; i < before.size(); i++)
    {
        if (moved[i].x != before[i].x)
        {
            anyMoved = true;
            break;
        }
    }
    CHECK(anyMoved);

    // And it has to agree with a rebuild, point for point.
    std::vector<Vec2D> held(moved.begin(), moved.end());
    forceRebuild(artboard.get());
    auto fresh = shape->pathComposer()->localPath()->rawPath()->points();
    REQUIRE(fresh.count() == held.size());
    const double allowed = allowedDelta(shape);
    size_t violations = 0;
    for (size_t i = 0; i < held.size(); i++)
    {
        if (std::max(std::abs((double)held[i].x - (double)fresh[i].x),
                     std::abs((double)held[i].y - (double)fresh[i].y)) >
            allowed)
        {
            violations++;
        }
    }
    CHECK(violations == 0);
}
