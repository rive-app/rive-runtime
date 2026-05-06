#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/math/transform_components.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/text/text.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_color.hpp"
#include "rive/viewmodel/viewmodel_instance_enum.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive/viewmodel/viewmodel_instance_symbol_list_index.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/nested_artboard.hpp"
#include "utils/no_op_factory.hpp"
#include "utils/serializing_factory.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("Stateful Component ViewModelInstance", "[silver]")
{
    rive::SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/component_stateful_vm_instance.riv", &silver);

    auto artboard = file->artboardNamed("ParentArtboard");
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    auto heightProperty = vmi->propertyValue("h");
    REQUIRE(heightProperty != nullptr);
    REQUIRE(heightProperty->is<rive::ViewModelInstanceNumber>());
    heightProperty->as<rive::ViewModelInstanceNumber>()->propertyValue(200);

    int frames = 30;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }
    heightProperty->as<rive::ViewModelInstanceNumber>()->propertyValue(50);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("component_stateful_vm_instance"));
}

TEST_CASE("Stateful Component ViewModelInstance multi", "[silver]")
{
    rive::SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/component_stateful_vm_instance_2.riv", &silver);

    auto artboard = file->artboardNamed("Artboard");
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    auto labelProperty = vmi->propertyValue("label");
    REQUIRE(labelProperty != nullptr);
    REQUIRE(labelProperty->is<rive::ViewModelInstanceString>());

    int frames = 30;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }
    labelProperty->as<rive::ViewModelInstanceString>()->propertyValue(
        "Override");
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("component_stateful_vm_instance_2"));
}

TEST_CASE("Stateful Component multi-property independence", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/stateful_multi_property.riv", &silver);

    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);
    REQUIRE(vmi != nullptr);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    // Sanity: all parent props exist with expected types.
    auto btn1Count = vmi->propertyValue("btn1Count");
    auto btn1Tint = vmi->propertyValue("btn1Tint");
    auto btn1Label = vmi->propertyValue("btn1Label");
    auto btn1Clip = vmi->propertyValue("btn1Clip");
    auto btn1Display = vmi->propertyValue("btn1Display");
    auto btn2Count = vmi->propertyValue("btn2Count");
    auto btn2Tint = vmi->propertyValue("btn2Tint");
    auto btn2Label = vmi->propertyValue("btn2Label");
    auto btn2Clip = vmi->propertyValue("btn2Clip");
    auto btn2Display = vmi->propertyValue("btn2Display");
    REQUIRE(btn1Count != nullptr);
    REQUIRE(btn1Count->is<rive::ViewModelInstanceNumber>());
    REQUIRE(btn1Tint != nullptr);
    REQUIRE(btn1Tint->is<rive::ViewModelInstanceColor>());
    REQUIRE(btn1Label != nullptr);
    REQUIRE(btn1Label->is<rive::ViewModelInstanceString>());
    REQUIRE(btn1Clip != nullptr);
    REQUIRE(btn1Clip->is<rive::ViewModelInstanceBoolean>());
    REQUIRE(btn1Display != nullptr);
    REQUIRE(btn1Display->is<rive::ViewModelInstanceEnum>());
    REQUIRE(btn2Count != nullptr);
    REQUIRE(btn2Count->is<rive::ViewModelInstanceNumber>());
    REQUIRE(btn2Tint != nullptr);
    REQUIRE(btn2Tint->is<rive::ViewModelInstanceColor>());
    REQUIRE(btn2Label != nullptr);
    REQUIRE(btn2Label->is<rive::ViewModelInstanceString>());
    REQUIRE(btn2Clip != nullptr);
    REQUIRE(btn2Clip->is<rive::ViewModelInstanceBoolean>());
    REQUIRE(btn2Display != nullptr);
    REQUIRE(btn2Display->is<rive::ViewModelInstanceEnum>());

    auto runFrames = [&](int frames) {
        for (int i = 0; i < frames; i++)
        {
            silver.addFrame();
            stateMachine->advanceAndApply(0.016f);
            artboard->draw(renderer.get());
        }
    };

    // Drive btn1 properties one by one — btn2 should remain unchanged
    // throughout. Each segment isolates a single property type so the
    // snapshot makes the binding visible per-type.
    btn1Count->as<rive::ViewModelInstanceNumber>()->propertyValue(180);
    runFrames(5);

    btn1Tint->as<rive::ViewModelInstanceColor>()->propertyValue(0xFFFF3344);
    runFrames(5);

    btn1Label->as<rive::ViewModelInstanceString>()->propertyValue("One");
    runFrames(5);

    btn1Clip->as<rive::ViewModelInstanceBoolean>()->propertyValue(true);
    runFrames(5);

    btn1Display->as<rive::ViewModelInstanceEnum>()->value(1);
    runFrames(5);

    // Now drive btn2 — btn1 should keep its modified values from above.
    btn2Count->as<rive::ViewModelInstanceNumber>()->propertyValue(60);
    runFrames(5);

    btn2Tint->as<rive::ViewModelInstanceColor>()->propertyValue(0xFF33AAFF);
    runFrames(5);

    btn2Label->as<rive::ViewModelInstanceString>()->propertyValue("Two");
    runFrames(5);

    btn2Clip->as<rive::ViewModelInstanceBoolean>()->propertyValue(true);
    runFrames(5);

    btn2Display->as<rive::ViewModelInstanceEnum>()->value(1);
    runFrames(5);

    CHECK(silver.matches("stateful_multi_property"));
}

TEST_CASE("Stateful Component nested in stateful", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/stateful_nested.riv", &silver);

    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);
    REQUIRE(vmi != nullptr);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    auto btn1Label = vmi->propertyValue("btn1Label");
    auto btn2Label = vmi->propertyValue("btn2Label");
    auto btn1Tint = vmi->propertyValue("btn1Tint");
    auto btn2Tint = vmi->propertyValue("btn2Tint");
    REQUIRE(btn1Label != nullptr);
    REQUIRE(btn1Label->is<rive::ViewModelInstanceString>());
    REQUIRE(btn2Label != nullptr);
    REQUIRE(btn2Label->is<rive::ViewModelInstanceString>());
    REQUIRE(btn1Tint != nullptr);
    REQUIRE(btn1Tint->is<rive::ViewModelInstanceColor>());
    REQUIRE(btn2Tint != nullptr);
    REQUIRE(btn2Tint->is<rive::ViewModelInstanceColor>());

    auto runFrames = [&](int frames) {
        for (int i = 0; i < frames; i++)
        {
            silver.addFrame();
            stateMachine->advanceAndApply(0.016f);
            artboard->draw(renderer.get());
        }
    };

    // Drive each property in isolation. Each segment exercises a single
    // input on the outer Main VM that is wired down through MultiButton's
    // stateful boundary into one of its two inner Button instances.
    btn1Label->as<rive::ViewModelInstanceString>()->propertyValue("One");
    runFrames(5);

    btn1Tint->as<rive::ViewModelInstanceColor>()->propertyValue(0xFFFF3344);
    runFrames(5);

    btn2Label->as<rive::ViewModelInstanceString>()->propertyValue("Two");
    runFrames(5);

    btn2Tint->as<rive::ViewModelInstanceColor>()->propertyValue(0xFF33AAFF);
    runFrames(5);

    CHECK(silver.matches("stateful_nested"));
}

TEST_CASE("Stateful Component list with input/output bridge binds", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/stateful_list_props.riv", &silver);

    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);
    REQUIRE(vmi != nullptr);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    auto buttonsProp = vmi->propertyValue("buttons");
    REQUIRE(buttonsProp != nullptr);
    REQUIRE(buttonsProp->is<rive::ViewModelInstanceList>());
    auto buttonsList = buttonsProp->as<rive::ViewModelInstanceList>();
    REQUIRE(buttonsList != nullptr);

    // Helper: build a fresh ButtonVM instance with the given inputs.
    auto makeButton =
        [&](float count, uint32_t tint, const std::string& label, bool clip) {
            auto inst = file->createViewModelInstance("ButtonVM");
            REQUIRE(inst != nullptr);
            auto pCount = inst->propertyValue("count");
            auto pTint = inst->propertyValue("tint");
            auto pLabel = inst->propertyValue("label");
            auto pClip = inst->propertyValue("clipped");
            REQUIRE(pCount != nullptr);
            REQUIRE(pTint != nullptr);
            REQUIRE(pLabel != nullptr);
            REQUIRE(pClip != nullptr);
            REQUIRE(pCount->is<rive::ViewModelInstanceNumber>());
            REQUIRE(pTint->is<rive::ViewModelInstanceColor>());
            REQUIRE(pLabel->is<rive::ViewModelInstanceString>());
            REQUIRE(pClip->is<rive::ViewModelInstanceBoolean>());
            pCount->as<rive::ViewModelInstanceNumber>()->propertyValue(count);
            pTint->as<rive::ViewModelInstanceColor>()->propertyValue(tint);
            pLabel->as<rive::ViewModelInstanceString>()->propertyValue(label);
            pClip->as<rive::ViewModelInstanceBoolean>()->propertyValue(clip);
            return inst;
        };

    auto addItem = [&](rive::rcp<rive::ViewModelInstance> inst) {
        auto item = rive::make_rcp<rive::ViewModelInstanceListItem>();
        item->viewModelInstance(inst);
        buttonsList->addItem(item);
    };

    // Three buttons with distinct inputs. We keep handles to the
    // *original* (user-supplied) ButtonVM instances — these are the ones
    // whose `click` trigger is written back via the output bridge bind
    // (clone -> original) when the corresponding rendered button is
    // pressed.
    auto button0 = makeButton(20, 0xFFFF3344, "Alpha", false);
    auto button1 = makeButton(40, 0xFF33AAFF, "Beta", false);
    auto button2 = makeButton(60, 0xFF44CC55, "Gamma", false);
    addItem(button0);
    addItem(button1);
    addItem(button2);

    auto runFrames = [&](int frames) {
        for (int i = 0; i < frames; i++)
        {
            silver.addFrame();
            stateMachine->advanceAndApply(0.016f);
            artboard->draw(renderer.get());
        }
    };

    // Settle the list with all three items visible.
    runFrames(5);

    auto getClickedBool = [&](rive::rcp<rive::ViewModelInstance> btn) {
        auto p = btn->propertyValue("clicked");
        REQUIRE(p != nullptr);
        REQUIRE(p->is<rive::ViewModelInstanceBoolean>());
        return p->as<rive::ViewModelInstanceBoolean>();
    };
    auto clicked0 = getClickedBool(button0);
    auto clicked1 = getClickedBool(button1);
    auto clicked2 = getClickedBool(button2);

    // To observe trigger fires on the original VMs we attach a delegate.
    // Reading `propertyValue()` post-frame doesn't work for triggers
    // because `ViewModelInstanceTrigger::advanced()` resets the value
    // back to 0 every frame — that's their normal "fired this frame"
    // semantic. The delegate's `valueChanged` is invoked in
    // `propertyValueChanged()`, before the reset, so it captures real
    // fires.
    struct FireCounter : public rive::ViewModelInstanceValueDelegate
    {
        int count = 0;
        void valueChanged() override { count++; }
    };
    FireCounter fire0, fire1, fire2;
    auto getClickTrigger = [&](rive::rcp<rive::ViewModelInstance> btn) {
        auto p = btn->propertyValue("click");
        REQUIRE(p != nullptr);
        REQUIRE(p->is<rive::ViewModelInstanceTrigger>());
        return p->as<rive::ViewModelInstanceTrigger>();
    };
    auto trigger0 = getClickTrigger(button0);
    auto trigger1 = getClickTrigger(button1);
    auto trigger2 = getClickTrigger(button2);
    trigger0->addDelegate(&fire0);
    trigger1->addDelegate(&fire1);
    trigger2->addDelegate(&fire2);

    // --- Input bridge: modify inputs on the originals; the cloned VMIs
    // inside the list should pick up the new values. ---
    button1->propertyValue("label")
        ->as<rive::ViewModelInstanceString>()
        ->propertyValue("Beta!");
    runFrames(5);

    button2->propertyValue("tint")
        ->as<rive::ViewModelInstanceColor>()
        ->propertyValue(0xFFFFCC00);
    runFrames(5);

    button0->propertyValue("count")
        ->as<rive::ViewModelInstanceNumber>()
        ->propertyValue(80);
    runFrames(5);

    // --- Output bridge: clicks on the rendered list items must
    // propagate the `clicked` boolean back to the originals. ---
    // Layout: 10px padding, ~35px button height, 10px gap, vertical.
    // Button 0 center y = 10 + 35/2 = 27.5
    // Button 1 center y = 10 + 35 + 10 + 35/2 = 72.5
    // Button 2 center y = 10 + 2*(35+10) + 35/2 = 117.5

    // Sanity: nothing has been clicked yet.
    CHECK(clicked0->propertyValue() == false);
    CHECK(clicked1->propertyValue() == false);
    CHECK(clicked2->propertyValue() == false);
    CHECK(fire0.count == 0);
    CHECK(fire1.count == 0);
    CHECK(fire2.count == 0);

    // Click button 0 — pointerDown + pointerUp completes the click, then
    // we advance once and read `clicked` and the trigger fire count.
    stateMachine->pointerDown(rive::Vec2D(50.0f, 27.0f));
    stateMachine->pointerUp(rive::Vec2D(50.0f, 27.0f));
    stateMachine->advanceAndApply(0.016f);
    CHECK(clicked0->propertyValue() == true);
    CHECK(clicked1->propertyValue() == false);
    CHECK(clicked2->propertyValue() == false);
    int fire0After1 = fire0.count;
    int fire1After1 = fire1.count;
    int fire2After1 = fire2.count;
    CHECK(fire0After1 > 0);
    CHECK(fire1After1 == 0);
    CHECK(fire2After1 == 0);
    runFrames(3);

    // Click button 1.
    stateMachine->pointerDown(rive::Vec2D(50.0f, 73.0f));
    stateMachine->pointerUp(rive::Vec2D(50.0f, 73.0f));
    stateMachine->advanceAndApply(0.016f);
    CHECK(clicked1->propertyValue() == true);
    CHECK(fire0.count == fire0After1);
    CHECK(fire1.count > fire1After1);
    CHECK(fire2.count == fire2After1);
    int fire0After2 = fire0.count;
    int fire1After2 = fire1.count;
    int fire2After2 = fire2.count;
    runFrames(3);

    // Click button 2.
    stateMachine->pointerDown(rive::Vec2D(50.0f, 118.0f));
    stateMachine->pointerUp(rive::Vec2D(50.0f, 118.0f));
    stateMachine->advanceAndApply(0.016f);
    CHECK(clicked2->propertyValue() == true);
    CHECK(fire0.count == fire0After2);
    CHECK(fire1.count == fire1After2);
    CHECK(fire2.count > fire2After2);
    runFrames(3);

    CHECK(silver.matches("stateful_list_props"));
}

TEST_CASE("Stateful Component dynamic artboard swap via VMI artboard "
          "property",
          "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/stateful_artboard_swap.riv", &silver);

    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);
    REQUIRE(vmi != nullptr);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    auto buttonArtboardProp = vmi->propertyValue("buttonArtboard");
    REQUIRE(buttonArtboardProp != nullptr);
    REQUIRE(buttonArtboardProp->is<rive::ViewModelInstanceArtboard>());
    auto vmiArtboard =
        buttonArtboardProp->as<rive::ViewModelInstanceArtboard>();

    auto buttonSource = file->bindableArtboardNamed("Button");
    auto strokedButtonSource = file->bindableArtboardNamed("StrokedButton");
    REQUIRE(buttonSource != nullptr);
    REQUIRE(strokedButtonSource != nullptr);

    auto runFrames = [&](int frames) {
        for (int i = 0; i < frames; i++)
        {
            silver.addFrame();
            stateMachine->advanceAndApply(0.016f);
            artboard->draw(renderer.get());
        }
    };

    // Helper: navigate to the swap-target NestedArtboard's currently-bound
    // stateful VMI so we can verify it was created fresh after each swap.
    auto findSwappedNestedArtboard = [&]() -> rive::NestedArtboard* {
        for (auto na : artboard->nestedArtboards())
        {
            // The swap-target NA is the one whose source is driven by
            // `buttonArtboard`; the easiest way to recognize it is that
            // its current source artboard matches one of the variants.
            auto* src = na->sourceArtboard();
            if (src != nullptr &&
                (src->name() == "Button" || src->name() == "StrokedButton"))
            {
                return na;
            }
        }
        return nullptr;
    };

    // Initial: buttonArtboard is null, so nothing is rendered in the
    // swap target. Capture the empty baseline.
    runFrames(2);
    REQUIRE(findSwappedNestedArtboard() == nullptr);

    // Swap to Button.
    vmiArtboard->asset(buttonSource);
    runFrames(5);

    auto naAfterButton = findSwappedNestedArtboard();
    REQUIRE(naAfterButton != nullptr);
    REQUIRE(naAfterButton->sourceArtboard() != nullptr);
    CHECK(naAfterButton->sourceArtboard()->name() == "Button");

    // Swap to StrokedButton — exercises updateArtboard's dispose+rebuild
    // path. The stateful artboard should change types entirely; the
    // previous Button stateful VMI should be torn down.
    vmiArtboard->asset(strokedButtonSource);
    runFrames(5);

    auto naAfterStroked = findSwappedNestedArtboard();
    REQUIRE(naAfterStroked != nullptr);
    REQUIRE(naAfterStroked->sourceArtboard() != nullptr);
    CHECK(naAfterStroked->sourceArtboard()->name() == "StrokedButton");

    // Sanity: the StrokedButton instance has its VM-specific
    // `strokeWidth` property visible, but Button's VM does not.
    auto strokedAbInst = naAfterStroked->artboardInstance();
    REQUIRE(strokedAbInst != nullptr);
    auto strokedCtx = strokedAbInst->dataContext();
    REQUIRE(strokedCtx != nullptr);
    auto strokedVmi = strokedCtx->viewModelInstance();
    REQUIRE(strokedVmi != nullptr);
    auto strokeWidthProp = strokedVmi->propertyValue("strokeWidth");
    REQUIRE(strokeWidthProp != nullptr);
    REQUIRE(strokeWidthProp->is<rive::ViewModelInstanceNumber>());
    // Drive strokeWidth so the silver actually shows the stroke and we
    // can visually verify the StrokedButton variant rendered.
    strokeWidthProp->as<rive::ViewModelInstanceNumber>()->propertyValue(8);
    runFrames(5);

    // Swap back to Button — the previous StrokedButton stateful VMI
    // (with strokeWidth) must be cleaned up; we should land back on a
    // ButtonVM-shaped instance.
    vmiArtboard->asset(buttonSource);
    runFrames(5);

    auto naAfterButton2 = findSwappedNestedArtboard();
    REQUIRE(naAfterButton2 != nullptr);
    REQUIRE(naAfterButton2->sourceArtboard() != nullptr);
    CHECK(naAfterButton2->sourceArtboard()->name() == "Button");

    auto buttonAbInst = naAfterButton2->artboardInstance();
    REQUIRE(buttonAbInst != nullptr);
    auto buttonCtx = buttonAbInst->dataContext();
    REQUIRE(buttonCtx != nullptr);
    auto buttonVmi = buttonCtx->viewModelInstance();
    REQUIRE(buttonVmi != nullptr);
    // Button's VM has count/tint/label/clipped/clicked/click but
    // does NOT have strokeWidth.
    CHECK(buttonVmi->propertyValue("count") != nullptr);
    CHECK(buttonVmi->propertyValue("strokeWidth") == nullptr);

    // Swap back to null — the swap target should disappear again.
    // asset(nullptr) calls propertyValue(-1) internally, which together
    // satisfy the cleared-state condition in NestedArtboard::updateArtboard.
    vmiArtboard->asset(nullptr);
    runFrames(5);
    CHECK(findSwappedNestedArtboard() == nullptr);

    // Swap back to Button after the null-clear. Verifies that the
    // cleared-state path actually drops the cached stateful VMI — if it
    // didn't, this swap would land on a VMI that was held over from the
    // previous variant's lifetime instead of a freshly created one. We
    // can't easily assert ptr-inequality without holding refs, so we
    // settle for asserting the new VMI is valid and shaped like
    // ButtonVM.
    vmiArtboard->asset(buttonSource);
    runFrames(5);
    auto naAfterClear = findSwappedNestedArtboard();
    REQUIRE(naAfterClear != nullptr);
    REQUIRE(naAfterClear->sourceArtboard() != nullptr);
    CHECK(naAfterClear->sourceArtboard()->name() == "Button");
    auto buttonAbInst3 = naAfterClear->artboardInstance();
    REQUIRE(buttonAbInst3 != nullptr);
    auto buttonCtx3 = buttonAbInst3->dataContext();
    REQUIRE(buttonCtx3 != nullptr);
    auto buttonVmi3 = buttonCtx3->viewModelInstance();
    REQUIRE(buttonVmi3 != nullptr);
    CHECK(buttonVmi3->propertyValue("count") != nullptr);
    CHECK(buttonVmi3->propertyValue("strokeWidth") == nullptr);

    CHECK(silver.matches("stateful_artboard_swap"));
}

TEST_CASE("Stateful Component list bridge binds clean up on item remove",
          "[silver]")
{
    // Reuses stateful_list_props.riv. Verifies that adding and then
    // removing list items doesn't leak bridge binds or crash, and that
    // surviving items still propagate clicks correctly.
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/stateful_list_props.riv", &silver);

    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);
    REQUIRE(vmi != nullptr);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);

    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    auto buttonsList =
        vmi->propertyValue("buttons")->as<rive::ViewModelInstanceList>();
    REQUIRE(buttonsList != nullptr);

    auto makeAndAdd = [&](const std::string& label, uint32_t tint) {
        auto inst = file->createViewModelInstance("ButtonVM");
        REQUIRE(inst != nullptr);
        inst->propertyValue("label")
            ->as<rive::ViewModelInstanceString>()
            ->propertyValue(label);
        inst->propertyValue("tint")
            ->as<rive::ViewModelInstanceColor>()
            ->propertyValue(tint);
        auto item = rive::make_rcp<rive::ViewModelInstanceListItem>();
        item->viewModelInstance(inst);
        buttonsList->addItem(item);
        return inst;
    };

    auto runFrames = [&](int frames) {
        for (int i = 0; i < frames; i++)
        {
            silver.addFrame();
            stateMachine->advanceAndApply(0.016f);
            artboard->draw(renderer.get());
        }
    };

    auto buttonA = makeAndAdd("Alpha", 0xFFFF3344);
    auto buttonB = makeAndAdd("Beta", 0xFF33AAFF);
    auto buttonC = makeAndAdd("Gamma", 0xFF44CC55);
    runFrames(3);

    // Remove the middle item — Beta. Bridge binds for Beta must be torn
    // down; Alpha and Gamma must keep working.
    REQUIRE(buttonsList->listItems().size() == 3);
    buttonsList->removeItem(1);
    runFrames(5);
    REQUIRE(buttonsList->listItems().size() == 2);

    // Click survivor at the position now occupied by what was Gamma
    // (originally index 2, now index 1 → y ≈ 73).
    auto clickedC =
        buttonC->propertyValue("clicked")->as<rive::ViewModelInstanceBoolean>();
    auto clickedB =
        buttonB->propertyValue("clicked")->as<rive::ViewModelInstanceBoolean>();
    CHECK(clickedC->propertyValue() == false);
    stateMachine->pointerDown(rive::Vec2D(50.0f, 73.0f));
    stateMachine->pointerUp(rive::Vec2D(50.0f, 73.0f));
    stateMachine->advanceAndApply(0.016f);
    CHECK(clickedC->propertyValue() == true);
    // Removed item's clicked must not flip — its bridge bind is gone.
    CHECK(clickedB->propertyValue() == false);
    runFrames(3);

    // Re-add Beta at the end.
    {
        auto item = rive::make_rcp<rive::ViewModelInstanceListItem>();
        item->viewModelInstance(buttonB);
        buttonsList->addItem(item);
    }
    runFrames(5);
    REQUIRE(buttonsList->listItems().size() == 3);

    // Beta now lives at index 2 → y ≈ 118. Click should bridge-bind to
    // its `clicked` again.
    stateMachine->pointerDown(rive::Vec2D(50.0f, 118.0f));
    stateMachine->pointerUp(rive::Vec2D(50.0f, 118.0f));
    stateMachine->advanceAndApply(0.016f);
    CHECK(clickedB->propertyValue() == true);
    runFrames(3);

    // Remove all items — must not crash, no leftover binds.
    while (buttonsList->listItems().size() > 0)
    {
        buttonsList->removeItem(0);
    }
    runFrames(5);
    REQUIRE(buttonsList->listItems().size() == 0);

    CHECK(silver.matches("stateful_list_props_lifecycle"));
}
