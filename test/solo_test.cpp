#include <rive/solo.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/animation/state_machine_input_instance.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("file with skins in solos loads correctly", "[solo]")
{
    auto file = ReadRiveFile("../../test/assets/death_knight.riv");

    auto artboard = file->artboard()->instance();
    artboard->advance(0.0f);
    auto solos = artboard->find<rive::Solo>();
    REQUIRE(solos.size() == 2);
}

TEST_CASE("children load correctly", "[solo]")
{
    auto file = ReadRiveFile("../../test/assets/solo_test.riv");

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
    auto file = ReadRiveFile("../../test/assets/nested_solo.riv");

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
    auto file = ReadRiveFile("../../test/assets/hit_test_solos.riv");

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
