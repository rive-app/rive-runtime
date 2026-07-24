#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include <utils/no_op_renderer.hpp>
#include "rive/animation/state_machine_instance.hpp"
#include "utils/serializing_factory.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

using namespace rive;

TEST_CASE("Computed root values", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/computed_values_test.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    stateMachine->bindViewModelInstance(vmi);

    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    int frames = (int)(2.0f / 0.032f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.032f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("computed_values_test"));
}

TEST_CASE("Image computed width/height track layout resize", "[silver]")
{
    // Two images sit in layouts animated from 150x150 to 200x200 (fit: resize)
    // and 300x250 (fit: fitHeight). Their computedWidth/computedHeight are
    // bound to view model numbers which in turn drive two ellipse sizes.
    // Regression: C++ Image lacked computedWidth/Height overrides, so the
    // bound values stayed 0.
    SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/image_computed_transform_bind.riv", &silver);

    auto artboard = file->artboardDefault();
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    stateMachine->bindViewModelInstance(vmi);

    auto number = [&](const char* name) {
        return vmi->propertyValue(name)
            ->as<ViewModelInstanceNumber>()
            ->propertyValue();
    };

    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    // Both layouts start at 150x150; both fits resolve to a 150x150 image.
    CHECK(number("img1Width") == Approx(150.0f).margin(5.0f));
    CHECK(number("img1Height") == Approx(150.0f).margin(5.0f));
    CHECK(number("img2Width") == Approx(150.0f).margin(5.0f));
    CHECK(number("img2Height") == Approx(150.0f).margin(5.0f));

    int frames = (int)(2.0f / 0.032f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.032f);
        artboard->draw(renderer.get());
    }

    // One-shot timeline has settled: fit resize tracks the 200x200 layout;
    // fitHeight scales both axes by layoutHeight/imageHeight = 250/128.
    CHECK(number("img1Width") == Approx(200.0f).margin(0.01f));
    CHECK(number("img1Height") == Approx(200.0f).margin(0.01f));
    CHECK(number("img2Width") == Approx(250.0f).margin(0.01f));
    CHECK(number("img2Height") == Approx(250.0f).margin(0.01f));

    CHECK(silver.matches("image_computed_transform_bind"));
}
