#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/shapes/paint/stroke.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/shapes/paint/stroke_effect.hpp>
#include <rive/shapes/paint/color.hpp>
#include <utils/no_op_renderer.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("A 0 scale path will trim with no crash", "[file]")
{
    auto file = ReadRiveFile("assets/trim.riv");

    auto artboard = file->artboard();
    auto node = artboard->find<rive::Node>("I");
    REQUIRE(node != nullptr);
    REQUIRE(node->scaleX() != 0);
    REQUIRE(node->scaleY() != 0);

    artboard->advance(0.0f);

    rive::NoOpRenderer renderer;
    artboard->draw(&renderer);

    // Now set scale to 0 and make sure it doesn't crash.
    node->scaleX(0.0f);
    node->scaleY(0.0f);
    artboard->advance(0.0f);
    artboard->draw(&renderer);
}

static void testRawPath(rive::Artboard* artboard,
                        std::string shapeName,
                        std::vector<rive::PathVerb> verbs)
{
    auto node = artboard->find<rive::Shape>(shapeName);
    rive::Stroke* stroke;
    for (auto& child : node->children())
    {
        if (child->is<rive::Stroke>())
        {
            stroke = child->as<rive::Stroke>();
            break;
        }
    }
    REQUIRE(stroke != nullptr);
    REQUIRE(stroke->is<rive::Stroke>());
    auto effect = stroke->effect();
    REQUIRE(effect != nullptr);
    auto effectPath = effect->effectPath();
    REQUIRE(effectPath != nullptr);
    auto rawPath = effectPath->rawPath();
    REQUIRE(rawPath != nullptr);
    size_t index = 0;
    for (auto iter : *rawPath)
    {
        REQUIRE(verbs.size() > index);
        rive::PathVerb verb = std::get<0>(iter);
        REQUIRE(verb == verbs[index]);
        index++;
    }
    REQUIRE(verbs.size() == index);
}

TEST_CASE("Different types of trim paths", "[trim]")
{
    auto file = ReadRiveFile("assets/trim_path.riv");

    auto artboard = file->artboard("artboard-2");
    REQUIRE(artboard != nullptr);
    artboard->updateComponents();
    // Closed Path with start at 50%, end at 100% and offset at 75% doesn't add
    // an extra move between end and start of path
    // This is two quarter circle lines joined together
    std::vector<rive::PathVerb> verbs1 = {rive::PathVerb::move,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line};
    testRawPath(artboard, "clipped-rect", verbs1);
    // Open Path with start at 50%, end at 100% and offset at 75% does add
    // an extra move between end and start of path
    std::vector<rive::PathVerb> verbs2 = {rive::PathVerb::move,
                                          rive::PathVerb::line,
                                          rive::PathVerb::move,
                                          rive::PathVerb::line};
    testRawPath(artboard, "clipped-rect-open", verbs2);
    // Multiple closed paths with start at 0%, end at 50% and offset at 90% do
    // move between paths
    std::vector<rive::PathVerb> verbs3 = {rive::PathVerb::move,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::move,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::close};
    testRawPath(artboard, "clipped-rect-multi", verbs3);
    // Multiple closed paths with trim path synced with start at 0%, end at 50%
    // and offset at 90% do move between paths
    std::vector<rive::PathVerb> verbs4 = {rive::PathVerb::move,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::move,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line};
    testRawPath(artboard, "clipped-rect-multi-sync", verbs4);
    // Open Shape with trim path at 0% 100% 0% does not close
    std::vector<rive::PathVerb> verbs5 = {rive::PathVerb::move,
                                          rive::PathVerb::cubic,
                                          rive::PathVerb::cubic};
    testRawPath(artboard, "pen-shape", verbs5);
    // Closed Shape with trim path at 0% 100% 0% does close
    std::vector<rive::PathVerb> verbs6 = {rive::PathVerb::move,
                                          rive::PathVerb::cubic,
                                          rive::PathVerb::cubic,
                                          rive::PathVerb::cubic,
                                          rive::PathVerb::close};
    testRawPath(artboard, "pen-shape-close", verbs6);
    // Mixed shapes with open and closed paths respect their close value
    std::vector<rive::PathVerb> verbs7 = {rive::PathVerb::move,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::move,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::close,
                                          rive::PathVerb::move,
                                          rive::PathVerb::cubic,
                                          rive::PathVerb::cubic,
                                          rive::PathVerb::cubic};
    testRawPath(artboard, "mixed-shapes", verbs7);
    // Mixed shapes with synced trim path with open and closed paths respect
    // their close value
    std::vector<rive::PathVerb> verbs8 = {rive::PathVerb::move,
                                          rive::PathVerb::cubic,
                                          rive::PathVerb::cubic,
                                          rive::PathVerb::move,
                                          rive::PathVerb::cubic,
                                          rive::PathVerb::cubic,
                                          rive::PathVerb::move,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::move,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line,
                                          rive::PathVerb::line};
    testRawPath(artboard, "mixed-shapes-synced", verbs8);
    // Mixed shapes with synced trim path with open and closed paths respect
    // their close value at 100%
    std::vector<rive::PathVerb> verbs9 = {
        rive::PathVerb::move, rive::PathVerb::cubic, rive::PathVerb::cubic,
        rive::PathVerb::move, rive::PathVerb::cubic, rive::PathVerb::cubic,
        rive::PathVerb::move, rive::PathVerb::line,  rive::PathVerb::line,
        rive::PathVerb::line, rive::PathVerb::line,  rive::PathVerb::line,
        rive::PathVerb::line, rive::PathVerb::close, rive::PathVerb::move,
        rive::PathVerb::line, rive::PathVerb::line,  rive::PathVerb::line,
        rive::PathVerb::line, rive::PathVerb::line,  rive::PathVerb::close};
    testRawPath(artboard, "mixed-shapes-synced-100", verbs9);
    // Mixed shapes with open and closed paths respect their close value at 100%
    std::vector<rive::PathVerb> verbs10 = {rive::PathVerb::move,
                                           rive::PathVerb::line,
                                           rive::PathVerb::line,
                                           rive::PathVerb::line,
                                           rive::PathVerb::line,
                                           rive::PathVerb::line,
                                           rive::PathVerb::move,
                                           rive::PathVerb::line,
                                           rive::PathVerb::line,
                                           rive::PathVerb::line,
                                           rive::PathVerb::line,
                                           rive::PathVerb::line,
                                           rive::PathVerb::close,
                                           rive::PathVerb::move,
                                           rive::PathVerb::cubic,
                                           rive::PathVerb::cubic,
                                           rive::PathVerb::cubic};
    testRawPath(artboard, "mixed-shapes-100", verbs10);
}
