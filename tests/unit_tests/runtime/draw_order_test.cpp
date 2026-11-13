#include <rive/file.hpp>
#include <rive/draw_rules.hpp>
#include <rive/draw_target.hpp>
#include <rive/node.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/animation/linear_animation_instance.hpp>
#include "utils/no_op_renderer.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>
#include <string>
#include <vector>

TEST_CASE("draw rules load and sort correctly", "[draw rules]")
{
  auto file = ReadRiveFile("assets/draw_rule_cycle.riv");

  // auto file = reader.file();
  std::unique_ptr<rive::ArtboardInstance> artboard = file->artboardDefault();
  auto node = artboard->find<rive::Node>("Blue");
  REQUIRE(node != nullptr);
  REQUIRE(node->is<rive::Shape>());
  // auto shape = node->as<rive::Shape>();

  artboard->updateComponents();
  REQUIRE(artboard->animationCount() == 1);

  // Check that we can advance the ping-pong animation with 1 second duration
  // without a hang.
  std::unique_ptr<rive::LinearAnimationInstance>
      animation = artboard->animationAt(0);
  // Advance and apply some frames.
  int frames = 10;
  float frameDuration = 1.0f;

  for (int i = 0; i < frames; i++)
  {
    animation->advanceAndApply(frameDuration);
    rive::NoOpRenderer renderer;
    artboard->draw(&renderer);
  }
}

TEST_CASE("draw targets without a drawable are dropped on load", "[draw rules]")
{
  // draw_rule_cycle.riv with drawableId stripped from every DrawTarget, which
  // is what the editor exports when a rule targets a drawable that doesn't
  // export (e.g. one in a guide layer). Used to crash Artboard::initialize.
  auto file = ReadRiveFile("assets/draw_target_missing_drawable.riv");

  std::unique_ptr<rive::ArtboardInstance> artboard = file->artboardDefault();
  REQUIRE(artboard->find<rive::DrawTarget>().empty());
  auto rules = artboard->find<rive::DrawRules>();
  REQUIRE(!rules.empty());
  for (auto rule : rules)
  {
    CHECK(rule->activeTarget() == nullptr);
  }

  std::unique_ptr<rive::LinearAnimationInstance>
      animation = artboard->animationAt(0);
  for (int i = 0; i < 10; i++)
  {
    animation->advanceAndApply(1.0f);
    rive::NoOpRenderer renderer;
    artboard->draw(&renderer);
  }
}

// Each draw target as "<rule owner>/<drawable> <placement>: <its dependents in
// order>". The artboard's draw target order is sorted from exactly these lists.
static std::vector<std::string> drawTargetGraph(
    rive::ArtboardInstance* artboard)
{
  auto describe = [](rive::Component* component) -> std::string {
    if (!component->is<rive::DrawTarget>())
    {
      return "?";
    }
    auto target = component->as<rive::DrawTarget>();
    // A target's parent is its DrawRules, whose parent owns the rule.
    return target->parent()->parent()->name() + "/" +
           target->drawable()->name() +
           (target->placement() == rive::DrawTargetPlacement::before
                ? " before"
                : " after");
  };
  std::vector<std::string> graph;
  for (auto target : artboard->find<rive::DrawTarget>())
  {
    std::string line = describe(target) + ":";
    for (auto dependent : target->dependents())
    {
      line += " " + describe(dependent);
    }
    graph.push_back(line);
  }
  return graph;
}

TEST_CASE(
    "draw targets under another rule depend on that rule's targets",
    "[draw rules]")
{
  // Bullet Man has 10 draw targets, 7 of them on drawables that another rule
  // already places, so their order comes from these dependencies.
  auto file = ReadRiveFile("assets/bullet_man.riv");
  auto artboard = file->artboardNamed("Bullet Man");
  REQUIRE(artboard != nullptr);

  std::vector<std::string> expected = {
      "Fire_target/ before:",
      "Helmet_back/Cape after:",
      "Helmet_back/Cannon_front after:",
      "Hand_target/Cape after:",
      "Hand_target/Moustache after:",
      "Hand_target/Cannon_front after:",
      "Hand_target/Eyebrow_right before:",
      "Moustache/Eyebrow_right after: Hand_target/Moustache after",
      "Arm_left/Body_character after: Fire_target/ before",
      "Character/Cannon_front after: Hand_target/Cape after "
      "Hand_target/Eyebrow_right before Helmet_back/Cape after "
      "Moustache/Eyebrow_right after Arm_left/Body_character after",
  };
  CHECK(drawTargetGraph(artboard.get()) == expected);
}

TEST_CASE(
    "a component given two draw rules is ordered by the last one",
    "[draw rules]")
{
  rive::Artboard artboard(&gNoOpFactory);
  artboard.addObject(&artboard);
  auto add = [&](rive::Component* object, rive::Core* parent) {
    artboard.addObject(object);
    object->parentId(artboard.idOf(parent));
  };
  auto shape = [](const char* name) {
    auto shape = new rive::Shape();
    shape->name(name);
    return shape;
  };
  auto back = shape("back");
  add(back, &artboard);
  auto front = shape("front");
  add(front, &artboard);
  auto group = new rive::Node();
  add(group, &artboard);
  add(shape("inner"), group);
  // Two rules on the group, each with a target. Only the last one applies.
  auto addRules = [&](rive::Drawable* drawable, rive::DrawTarget** target) {
    auto rules = new rive::DrawRules();
    add(rules, group);
    *target = new rive::DrawTarget();
    add(*target, rules);
    (*target)->drawableId(artboard.idOf(drawable));
    return rules;
  };
  rive::DrawTarget* firstTarget = nullptr;
  rive::DrawTarget* lastTarget = nullptr;
  auto firstRules = addRules(back, &firstTarget);
  auto lastRules = addRules(front, &lastTarget);
  REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
  // The changed hooks need an artboard, so these go after initialize.
  firstRules->drawTargetId(artboard.idOf(firstTarget));
  lastRules->drawTargetId(artboard.idOf(lastTarget));
  artboard.advance(0.0f);

  std::vector<std::string> order;
  for (auto drawable = artboard.firstDrawable(); drawable != nullptr;
       drawable = drawable->prevDrawable())
  {
    order.push_back(drawable->name());
  }
  // The last rule places the group's contents before "front". Under the first
  // rule's target, which nothing orders, "inner" would not be drawn at all.
  CHECK(order == std::vector<std::string>{"front", "inner", "back"});
}
