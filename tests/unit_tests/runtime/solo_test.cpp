#include <rive/solo.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/shapes/path.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <rive/constraints/translation_constraint.hpp>
#include <rive/focus_data.hpp>
#include <rive/semantic/semantic_data.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/animation/state_machine_input_instance.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/nested_artboard_leaf.hpp>
#include <rive/shapes/paint/fill.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include "rive_file_reader.hpp"
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include "rive/viewmodel/viewmodel_instance_enum.hpp"
#include "rive/viewmodel/viewmodel_property_enum.hpp"
#include "rive/viewmodel/data_enum.hpp"
#include "utils/no_op_factory.hpp"
#include "utils/serializing_factory.hpp"
#include <catch.hpp>
#include <cstdint>
#include <cstdio>

TEST_CASE("file with skins in solos loads correctly", "[solo]")
{
    auto file = ReadRiveFile("assets/death_knight.riv");

    auto artboard = file->artboard()->instance();
    artboard->advance(0.0f);
    auto solos = artboard->find<rive::Solo>();
    REQUIRE(solos.size() == 2);
}

TEST_CASE("children load correctly", "[solo]")
{
    auto file = ReadRiveFile("assets/solo_test.riv");

    auto artboard = file->artboard()->instance();
    artboard->advance(0.0f);
    auto solos = artboard->find<rive::Solo>();
    REQUIRE(solos.size() == 1);
    auto solo = solos[0];
    REQUIRE(solo != nullptr);
    REQUIRE(solo->children().size() == 3);
    REQUIRE(solo->children()[0]->is<rive::Shape>());
    REQUIRE(solo->children()[0]->name() == "Blue");
    REQUIRE(solo->children()[1]->is<rive::Shape>());
    REQUIRE(solo->children()[1]->name() == "Green");
    REQUIRE(solo->children()[2]->is<rive::Shape>());
    REQUIRE(solo->children()[2]->name() == "Red");

    auto blue = solo->children()[0]->as<rive::Shape>();
    auto green = solo->children()[1]->as<rive::Shape>();
    auto red = solo->children()[2]->as<rive::Shape>();

    REQUIRE(!blue->isHidden());
    REQUIRE(green->isHidden());
    REQUIRE(red->isHidden());

    REQUIRE(green->children().size() == 2);
    REQUIRE(green->children()[0]->isCollapsed());
    REQUIRE(green->children()[1]->isCollapsed());

    REQUIRE(red->children().size() == 2);
    REQUIRE(red->children()[0]->isCollapsed());
    REQUIRE(red->children()[1]->isCollapsed());

    auto machine = artboard->defaultStateMachine();
    machine->advanceAndApply(0.0);
    // Red visible at start
    REQUIRE(blue->isHidden());
    REQUIRE(green->isHidden());
    REQUIRE(!red->isHidden());

    machine->advanceAndApply(0.5);
    // Green visible after 0.5 seconds.
    REQUIRE(blue->isHidden());
    REQUIRE(!green->isHidden());
    REQUIRE(red->isHidden());

    machine->advanceAndApply(0.5);
    // Blue visible at end
    REQUIRE(!blue->isHidden());
    REQUIRE(green->isHidden());
    REQUIRE(red->isHidden());
}

TEST_CASE("nested solos work", "[solo]")
{
    auto file = ReadRiveFile("assets/nested_solo.riv");

    auto artboard = file->artboard()->instance();
    artboard->advance(0.0f);
    auto s1 = artboard->find<rive::Solo>("Solo 1");
    REQUIRE(s1 != nullptr);
    auto s2 = artboard->find<rive::Solo>("Solo 2");
    REQUIRE(s2 != nullptr);
    auto s3 = artboard->find<rive::Solo>("Solo 3");
    REQUIRE(s3 != nullptr);

    auto a = artboard->find<rive::Shape>("A");
    REQUIRE(a != nullptr);
    auto b = artboard->find<rive::Shape>("B");
    REQUIRE(b != nullptr);
    auto c = artboard->find<rive::Shape>("C");
    REQUIRE(c != nullptr);
    auto d = artboard->find<rive::Shape>("D");
    REQUIRE(d != nullptr);
    auto e = artboard->find<rive::Shape>("E");
    REQUIRE(e != nullptr);
    auto f = artboard->find<rive::Shape>("F");
    REQUIRE(f != nullptr);
    auto g = artboard->find<rive::Shape>("G");
    REQUIRE(g != nullptr);
    auto h = artboard->find<rive::Shape>("H");
    REQUIRE(h != nullptr);
    auto i = artboard->find<rive::Shape>("I");
    REQUIRE(i != nullptr);

    s1->activeComponentId(artboard->idOf(a));
    s2->activeComponentId(artboard->idOf(d));
    s3->activeComponentId(artboard->idOf(h));
    artboard->advance(0.0f);

    REQUIRE(a->isCollapsed() == false);
    REQUIRE(b->isCollapsed() == true);
    REQUIRE(c->isCollapsed() == true);

    REQUIRE(d->isCollapsed() == true);
    REQUIRE(e->isCollapsed() == true);
    REQUIRE(f->isCollapsed() == true);

    REQUIRE(g->isCollapsed() == true);
    REQUIRE(h->isCollapsed() == true);
    REQUIRE(i->isCollapsed() == true);

    // Changing active in a collapsed solo doesn't affect anything.
    s3->activeComponentId(artboard->idOf(g));
    artboard->advance(0.0f);

    REQUIRE(a->isCollapsed() == false);
    REQUIRE(b->isCollapsed() == true);
    REQUIRE(c->isCollapsed() == true);

    REQUIRE(d->isCollapsed() == true);
    REQUIRE(e->isCollapsed() == true);
    REQUIRE(f->isCollapsed() == true);

    REQUIRE(g->isCollapsed() == true);
    REQUIRE(h->isCollapsed() == true);
    REQUIRE(i->isCollapsed() == true);

    s1->activeComponentId(artboard->idOf(c));
    artboard->advance(0.0);

    // Now the rest of the nested solo items should be visible.
    REQUIRE(a->isCollapsed() == true);
    REQUIRE(b->isCollapsed() == true);
    REQUIRE(c->isCollapsed() == false);

    REQUIRE(d->isCollapsed() == false);
    REQUIRE(e->isCollapsed() == true);
    REQUIRE(f->isCollapsed() == true);

    REQUIRE(g->isCollapsed() == false);
    REQUIRE(h->isCollapsed() == true);
    REQUIRE(i->isCollapsed() == true);
}

TEST_CASE("hit test on solos", "[solo]")
{
    auto file = ReadRiveFile("assets/hit_test_solos.riv");

    auto artboard = file->artboard()->instance();

    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->stateMachineCount() == 1);

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    stateMachine->advance(0.0f);
    artboard->advance(0.0f);

    auto toggle = stateMachine->getBool("hovered");
    REQUIRE(toggle != nullptr);

    // Inactive shape position
    stateMachine->pointerMove(rive::Vec2D(200.0f, 100.0f));
    REQUIRE(toggle->value() == true);

    // // Active shape position
    stateMachine->pointerMove(rive::Vec2D(200.0f, 300.0f));
    REQUIRE(toggle->value() == false);

    // // Inactive shape position
    stateMachine->pointerMove(rive::Vec2D(200.0f, 400.0f));
    REQUIRE(toggle->value() == false);

    // Switches active shape to middle one
    stateMachine->advance(1.5f);
    artboard->advance(1.5f);

    // Inactive shape position
    stateMachine->pointerMove(rive::Vec2D(200.0f, 100.0f));
    REQUIRE(toggle->value() == false);

    // // Active shape position
    stateMachine->pointerMove(rive::Vec2D(200.0f, 300.0f));
    REQUIRE(toggle->value() == true);

    // // Inactive shape position
    stateMachine->pointerMove(rive::Vec2D(200.0f, 400.0f));
    REQUIRE(toggle->value() == false);

    // Switches active shape to last one
    stateMachine->advance(1.0f);
    artboard->advance(1.0f);

    // Inactive shape position
    stateMachine->pointerMove(rive::Vec2D(200.0f, 100.0f));
    REQUIRE(toggle->value() == false);

    // // Inactive shape position
    stateMachine->pointerMove(rive::Vec2D(200.0f, 300.0f));
    REQUIRE(toggle->value() == false);

    // // Active shape position
    stateMachine->pointerMove(rive::Vec2D(200.0f, 400.0f));
    REQUIRE(toggle->value() == true);
}

TEST_CASE("hit test on nested artboards in solos", "[solo]")
{
    auto file =
        ReadRiveFile("assets/pointer_events_nested_artboards_in_solos.riv");

    auto mainArtboard = file->artboard()->instance();

    auto green_color = 0xFF00B511;
    auto red_color = 0xFFC80000;
    auto gray_color = 0xFF747474;

    REQUIRE(mainArtboard->find("Parent-Artboard") != nullptr);
    auto artboard = mainArtboard->find<rive::Artboard>("Parent-Artboard");

    REQUIRE(artboard != nullptr);
    artboard->updateComponents();
    REQUIRE(artboard->is<rive::Artboard>());
    REQUIRE(artboard->find("Nested-Artboard-Active") != nullptr);
    auto nestedArtboardActive =
        artboard->find<rive::NestedArtboard>("Nested-Artboard-Active");
    REQUIRE(nestedArtboardActive->artboardInstance() != nullptr);

    auto nestedArtboardActiveArtboardInstance =
        nestedArtboardActive->artboardInstance();
    auto activeRect = nestedArtboardActiveArtboardInstance->find<rive::Shape>(
        "Clickable-Rectangle");
    REQUIRE(activeRect != nullptr);
    auto activeRectFill = activeRect->children()[1]->as<rive::Fill>();
    REQUIRE(activeRectFill != nullptr);
    auto activeRectFillSolidColor =
        activeRectFill->paint()->as<rive::SolidColor>();
    REQUIRE(activeRectFillSolidColor != nullptr);

    REQUIRE(artboard->find("Nested-Artboard-Inactive") != nullptr);
    auto nestedArtboardInactive =
        artboard->find<rive::NestedArtboard>("Nested-Artboard-Inactive");
    REQUIRE(nestedArtboardInactive->artboardInstance() != nullptr);
    auto nestedArtboardInactiveArtboardInstance =
        nestedArtboardInactive->artboardInstance();
    auto inactiveRect =
        nestedArtboardInactiveArtboardInstance->find<rive::Shape>(
            "Clickable-Rectangle");
    REQUIRE(inactiveRect != nullptr);
    auto inactiveRectFill = inactiveRect->children()[1]->as<rive::Fill>();
    REQUIRE(inactiveRectFill != nullptr);
    auto inactiveRectFillSolidColor =
        inactiveRectFill->paint()->as<rive::SolidColor>();
    REQUIRE(inactiveRectFillSolidColor != nullptr);

    REQUIRE(artboard->stateMachineCount() == 1);

    auto stateMachine = mainArtboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    // Initialize state machine
    stateMachine->advance(0.0f);
    mainArtboard->advance(0.0f);

    REQUIRE(nestedArtboardActive->isCollapsed() == false);
    REQUIRE(nestedArtboardInactive->isCollapsed() == true);
    REQUIRE(activeRectFillSolidColor->colorValue() == green_color);
    REQUIRE(inactiveRectFillSolidColor->colorValue() == gray_color);

    // Advance to a position in the timeline where the inactive artboard
    // is active so it is rendered once
    stateMachine->advance(0.1f);
    mainArtboard->advance(0.1f);

    REQUIRE(nestedArtboardActive->isCollapsed() == true);
    REQUIRE(nestedArtboardInactive->isCollapsed() == false);
    REQUIRE(inactiveRectFillSolidColor->colorValue() == green_color);

    // Advance to a position in the timeline where the active artboard is active
    stateMachine->advance(0.1f);
    mainArtboard->advance(0.1f);

    REQUIRE(nestedArtboardActive->isCollapsed() == false);
    REQUIRE(nestedArtboardInactive->isCollapsed() == true);

    // Apply pointer up
    stateMachine->pointerUp(rive::Vec2D(200.0f, 200.0f));
    stateMachine->advance(0.0f);
    artboard->advance(0.0f);

    // Advance to activate the inactive artboard again so it redraws
    stateMachine->advance(0.1f);
    artboard->advance(0.1f);
    REQUIRE(nestedArtboardActive->isCollapsed() == true);
    REQUIRE(nestedArtboardInactive->isCollapsed() == false);

    // If the test succeeds the active nested artboard should have changed to
    // red
    REQUIRE(activeRectFillSolidColor->colorValue() == red_color);
    // And the inactive artboard should have stayed green
    REQUIRE(inactiveRectFillSolidColor->colorValue() == green_color);
}

TEST_CASE("solo index/name selection skips property-like children", "[solo]")
{
    // Build an artboard with a solo whose children interleave real solo options
    // (shapes) with property-like children (constraint, clipping shape, focus
    // data, semantic data). Those property-like children must be skipped by
    // index/name based selection so data binding targets only real options.
    rive::NoOpFactory factory;
    rive::Artboard artboard(&factory);

    auto* solo = new rive::Solo();
    auto* clip = new rive::ClippingShape();
    auto* constraint = new rive::TranslationConstraint();
    auto* blue = new rive::Shape();
    auto* focus = new rive::FocusData();
    auto* green = new rive::Shape();
    auto* semantic = new rive::SemanticData();
    auto* red = new rive::Shape();

    blue->name("Blue");
    green->name("Green");
    red->name("Red");

    artboard.addObject(&artboard);  // id 0
    artboard.addObject(solo);       // id 1
    artboard.addObject(clip);       // id 2
    artboard.addObject(constraint); // id 3
    artboard.addObject(blue);       // id 4
    artboard.addObject(focus);      // id 5
    artboard.addObject(green);      // id 6
    artboard.addObject(semantic);   // id 7
    artboard.addObject(red);        // id 8

    solo->parentId(0);
    clip->parentId(1);
    constraint->parentId(1);
    blue->parentId(1);
    focus->parentId(1);
    green->parentId(1);
    semantic->parentId(1);
    red->parentId(1);

    // The clipping shape needs a valid source node to initialize.
    clip->sourceId(artboard.idOf(blue));

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);

    // Index 0 must resolve to the first real option (Blue), NOT the clipping
    // shape that physically precedes it in the child list.
    solo->updateByIndex(0);
    REQUIRE(artboard.resolve(solo->activeComponentId()) == blue);
    REQUIRE(solo->getActiveChildIndex() == 0);

    solo->updateByIndex(1);
    REQUIRE(artboard.resolve(solo->activeComponentId()) == green);
    REQUIRE(solo->getActiveChildIndex() == 1);

    solo->updateByIndex(2);
    REQUIRE(artboard.resolve(solo->activeComponentId()) == red);
    REQUIRE(solo->getActiveChildIndex() == 2);
    REQUIRE(solo->getActiveChildName() == "Red");

    // Out of range for the solo set (there are only 3 options) is a no-op.
    solo->updateByIndex(3);
    REQUIRE(artboard.resolve(solo->activeComponentId()) == red);

    // An index past the whole child list (e.g. a negative float cast to size_t
    // by the data-binding path) is also a no-op.
    solo->updateByIndex(SIZE_MAX);
    REQUIRE(artboard.resolve(solo->activeComponentId()) == red);

    // Name based selection skips the property-like children too.
    solo->updateByName("Green");
    REQUIRE(artboard.resolve(solo->activeComponentId()) == green);
    REQUIRE(solo->getActiveChildIndex() == 1);

    // With Green active, only the non-active solo options collapse. The
    // property-like children follow the solo's own (uncollapsed) state.
    REQUIRE(blue->isCollapsed() == true);
    REQUIRE(green->isCollapsed() == false);
    REQUIRE(red->isCollapsed() == true);
    REQUIRE(clip->isCollapsed() == false);
    REQUIRE(constraint->isCollapsed() == false);
    REQUIRE(focus->isCollapsed() == false);
    REQUIRE(semantic->isCollapsed() == false);
}

TEST_CASE("Data bound solos with enums work in both directions", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/databind_solo_to_enum.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);
    REQUIRE(vmi != nullptr);
    REQUIRE(vmi->propertyValue("enuToSource") != nullptr);
    auto enuToSourceProp =
        vmi->propertyValue("enuToSource")->as<rive::ViewModelInstanceEnum>();
    CHECK(enuToSourceProp->propertyValue() == 3);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    silver.addFrame();

    stateMachine->pointerDown(rive::Vec2D(425.0f, 70.0f));
    stateMachine->pointerUp(rive::Vec2D(425.0f, 70.0f));
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    CHECK(enuToSourceProp->propertyValue() == 5);

    CHECK(silver.matches("databind_solo_to_enum"));
}

// The test asset carries Luau bytecode scripts, which only the Luau
// backend runs.
#ifdef WITH_RIVE_SCRIPTING_LUAU
TEST_CASE("Do not advance collapsed scripts", "[silver]")
{
    auto file = ReadRiveFile("assets/script_advance_test.riv");

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    stateMachine->bindViewModelInstance(vmi);
    auto soloIndexProp =
        vmi->propertyValue("soloIndex")->as<rive::ViewModelInstanceNumber>();
    auto advanceCountProp =
        vmi->propertyValue("advanceCount")->as<rive::ViewModelInstanceNumber>();

    REQUIRE(soloIndexProp->propertyValue() == 0);
    REQUIRE(advanceCountProp->propertyValue() == 0);
    stateMachine->advanceAndApply(0.016f);
    REQUIRE(advanceCountProp->propertyValue() == 1);
    stateMachine->advanceAndApply(0.016f);
    REQUIRE(advanceCountProp->propertyValue() == 2);

    // Toggles to another script
    soloIndexProp->propertyValue(1);
    stateMachine->advanceAndApply(0.016f);
    REQUIRE(advanceCountProp->propertyValue() == 3);

    // Toggles to a nested artboard with a script
    soloIndexProp->propertyValue(2);
    stateMachine->advanceAndApply(0.016f);
    REQUIRE(advanceCountProp->propertyValue() == 4);

    // Toggling to an index where no script advances
    soloIndexProp->propertyValue(3);
    stateMachine->advanceAndApply(0.016f);
    // Value updates once more because advance is always off-by-one frame to
    // the update cycle
    REQUIRE(advanceCountProp->propertyValue() == 5);

    stateMachine->advanceAndApply(0.016f);
    // Now script does not advance anymore
    REQUIRE(advanceCountProp->propertyValue() == 5);

    soloIndexProp->propertyValue(0);
    stateMachine->advanceAndApply(0.016f);
    REQUIRE(advanceCountProp->propertyValue() == 5);

    stateMachine->advanceAndApply(0.016f);
    REQUIRE(advanceCountProp->propertyValue() == 6);
}
#endif

TEST_CASE("Data bind by index skipping non hierarchical children", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/solo_index_test.riv", &silver);
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());
    auto renderer = silver.makeRenderer();

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    auto indexProp =
        vmi->propertyValue("index")->as<rive::ViewModelInstanceNumber>();

    stateMachine->bindViewModelInstance(vmi);

    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();

    indexProp->propertyValue(1);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();
    indexProp->propertyValue(2);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());
    silver.addFrame();
    indexProp->propertyValue(3);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("solo_index_test"));
}

// Every kind of child a Solo can hold, laid out by one parent layout: nested
// artboard leaves, plain and participating shapes/text/images. The view
// model's `states` enum picks the active one by name, so walking its values
// renders each child in turn.
TEST_CASE("solo children of a layout render for every state", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/layout/layout_solos.riv", &silver);

    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    auto viewModelId = artboard->viewModelId();
    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);
    REQUIRE(vmi != nullptr);
    stateMachine->bindViewModelInstance(vmi);

    auto statesValue = vmi->propertyValue("states");
    REQUIRE(statesValue != nullptr);
    auto states = statesValue->as<rive::ViewModelInstanceEnum>();

    auto enumProperty =
        states->viewModelProperty()->as<rive::ViewModelPropertyEnum>();
    auto dataEnum = enumProperty->dataEnum();
    REQUIRE(dataEnum != nullptr);
    REQUIRE(!dataEnum->values().empty());

    auto renderer = silver.makeRenderer();
    for (uint32_t i = 0; i < (uint32_t)dataEnum->values().size(); i++)
    {
        // The solo matches the enum value against its children's names and
        // silently keeps the current child when nothing matches, so assert the
        // write landed rather than re-rendering the previous state.
        REQUIRE(states->value(i));
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
        silver.addFrame();
    }

    CHECK(silver.matches("layout_solos"));
}

// The same scene as above with every leaf opted in to fitToLayoutParent. The
// asset was authored before the flag existed so its flag is false, which is
// what the sibling silver records; flipping it here is the only difference
// between the two, so the pair is the render-level statement of what the flag
// does. A leaf whose Solo has no layout above it is unaffected either way.
TEST_CASE("solo children of a layout render fitted to the layout parent",
          "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/layout/layout_solos.riv", &silver);

    auto artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto leaves = artboard->find<rive::NestedArtboardLeaf>();
    REQUIRE(!leaves.empty());
    for (auto* leaf : leaves)
    {
        REQUIRE(leaf->fitToLayoutParent() == false);
        leaf->fitToLayoutParent(true);
    }

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    auto viewModelId = artboard->viewModelId();
    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);
    REQUIRE(vmi != nullptr);
    stateMachine->bindViewModelInstance(vmi);

    auto statesValue = vmi->propertyValue("states");
    REQUIRE(statesValue != nullptr);
    auto states = statesValue->as<rive::ViewModelInstanceEnum>();

    auto enumProperty =
        states->viewModelProperty()->as<rive::ViewModelPropertyEnum>();
    auto dataEnum = enumProperty->dataEnum();
    REQUIRE(dataEnum != nullptr);
    REQUIRE(!dataEnum->values().empty());

    auto renderer = silver.makeRenderer();
    for (uint32_t i = 0; i < (uint32_t)dataEnum->values().size(); i++)
    {
        REQUIRE(states->value(i));
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
        silver.addFrame();
    }

    CHECK(silver.matches("layout_solos_fit_to_layout_parent"));
}

// solo_nested_artboard_leaf.riv holds the same 500x250 scene three ways, each
// nesting the "Item" artboard through a contain-fit NestedArtboardLeaf:
//
//   NoSolo                        leaf parented straight to the artboard
//   SoloWithLeafFitsToParentLayout  leaf under a Solo, fitToLayoutParent set
//   SoloWithLeaf                  leaf under a Solo, flag clear (legacy)
//
// The first two must render the same -- the flag is what lets sizing reach
// through the Solo to the artboard, so opting in restores what a direct child
// gets. The third must differ: with the flag clear the Solo stops the sizing
// and the leaf frames its own bounds, which is how every file written before
// the flag existed behaves.
// expectSolo/expectFitToLayoutParent assert what the asset was authored to
// hold, so a re-export that loses the flag fails here rather than quietly
// re-recording a silver that no longer tests anything.
static void renderSoloLeafArtboard(const char* artboardName,
                                   const char* silverName,
                                   bool expectSolo,
                                   bool expectFitToLayoutParent)
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/solo_nested_artboard_leaf.riv", &silver);

    auto artboard = file->artboardNamed(artboardName);
    REQUIRE(artboard != nullptr);
    silver.frameSize(artboard->width(), artboard->height());

    auto leaves = artboard->find<rive::NestedArtboardLeaf>();
    REQUIRE(leaves.size() == 1);
    auto* leaf = leaves[0];
    REQUIRE(leaf->parent() != nullptr);
    REQUIRE(leaf->parent()->is<rive::Solo>() == expectSolo);
    if (expectSolo)
    {
        // Only asserted under a Solo, which is the only place the flag decides
        // anything. Parented straight to the artboard both reaches find it, so
        // whatever the editor defaulted the flag to there is incidental.
        REQUIRE(leaf->fitToLayoutParent() == expectFitToLayoutParent);
    }
    REQUIRE(leaf->fit() == (uint8_t)rive::Fit::contain);

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    auto viewModelId = artboard->viewModelId();
    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);
    if (vmi != nullptr)
    {
        stateMachine->bindViewModelInstance(vmi);
    }

    auto renderer = silver.makeRenderer();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());
    silver.addFrame();

    CHECK(silver.matches(silverName));
}

TEST_CASE("a leaf parented by the artboard fits it", "[silver]")
{
    renderSoloLeafArtboard("NoSolo",
                           "solo_nested_artboard_leaf_no_solo",
                           /*expectSolo=*/false,
                           /*expectFitToLayoutParent=*/false); // unchecked
}

TEST_CASE("an opted-in leaf under a Solo fits the layout above it", "[silver]")
{
    renderSoloLeafArtboard("SoloWithLeafFitsToParentLayout",
                           "solo_nested_artboard_leaf_fits_parent_layout",
                           /*expectSolo=*/true,
                           /*expectFitToLayoutParent=*/true);
}

TEST_CASE("a legacy leaf under a Solo frames its own bounds", "[silver]")
{
    renderSoloLeafArtboard("SoloWithLeaf",
                           "solo_nested_artboard_leaf_solo",
                           /*expectSolo=*/true,
                           /*expectFitToLayoutParent=*/false);
}
