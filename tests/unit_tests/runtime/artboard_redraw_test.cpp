#include <rive/artboard.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/node.hpp>
#include <utils/no_op_factory.hpp>
#include <utils/no_op_renderer.hpp>
#include <catch.hpp>

using namespace rive;

namespace
{
constexpr float frameTime = 1.0f / 60.0f;

struct TransformScene
{
  NoOpFactory factory;
  Artboard artboard{&factory};
  NoOpRenderer renderer;
  Node* node = new Node();

  TransformScene()
  {
    artboard.addObject(&artboard);
    node->name("moving node");
    artboard.addObject(node);
    node->parentId(0);
    REQUIRE(artboard.initialize() == StatusCode::Ok);
    artboard.width(100);
    artboard.height(100);
    artboard.advance(frameTime);
    artboard.draw(&renderer);
    REQUIRE_FALSE(artboard.didChange());
  }
};
} // namespace

TEST_CASE(
    "a draw before pending transforms update does not consume their redraw",
    "[artboard][redraw]")
{
  TransformScene scene;
  const float before = scene.node->worldTransform()[4];
  scene.node->x(scene.node->x() + 20);
  REQUIRE(scene.artboard.didChange());

  // Mirrors a setter submitted after this frame's advance, before its draw.
  // Drawing consumes didChange but cannot apply the pending transform itself.
  scene.artboard.draw(&scene.renderer);
  REQUIRE_FALSE(scene.artboard.didChange());
  REQUIRE(scene.node->worldTransform()[4] == before);
  const auto serialAtPrematureDraw = scene.artboard.changeSerial();

  scene.artboard.advance(frameTime);
  REQUIRE(scene.node->worldTransform()[4] == Approx(before + 20));
  REQUIRE(scene.artboard.didChange());
  REQUIRE(scene.artboard.changeSerial() != serialAtPrematureDraw);

  scene.artboard.draw(&scene.renderer);
  REQUIRE_FALSE(scene.artboard.didChange());
  const auto settledSerial = scene.artboard.changeSerial();
  for (int i = 0; i < 3; ++i)
  {
    scene.artboard.advance(frameTime);
    CHECK_FALSE(scene.artboard.didChange());
    CHECK(scene.artboard.changeSerial() == settledSerial);
  }
}

TEST_CASE(
    "updating transforms after a premature nested draw also redraws the host",
    "[artboard][redraw]")
{
  TransformScene source;
  Artboard root(&source.factory);
  root.addObject(&root);
  auto* host = new NestedArtboard();
  root.addObject(host);
  host->parentId(0);
  REQUIRE(root.initialize() == StatusCode::Ok);
  root.width(100);
  root.height(100);
  host->referencedArtboard(
      source.artboard.instance<ArtboardInstance>().release());
  root.advance(frameTime);
  root.draw(&source.renderer);

  auto* nested = host->artboardInstance();
  REQUIRE(nested != nullptr);
  auto* node = nested->find<Node>("moving node");
  REQUIRE(node != nullptr);
  REQUIRE_FALSE(root.didChange());
  REQUIRE_FALSE(nested->didChange());

  const float before = node->worldTransform()[4];
  node->x(node->x() + 20);
  root.draw(&source.renderer);
  REQUIRE_FALSE(root.didChange());
  REQUIRE_FALSE(nested->didChange());
  REQUIRE(node->worldTransform()[4] == before);

  // Isolate the child's update: its changed notification must reach the host,
  // even when the host itself has no component work to perform.
  nested->advance(frameTime);
  CHECK(node->worldTransform()[4] == Approx(before + 20));
  CHECK(nested->didChange());
  CHECK(root.didChange());
  root.draw(&source.renderer);
  root.advance(frameTime);
  CHECK_FALSE(nested->didChange());
  CHECK_FALSE(root.didChange());
}

TEST_CASE(
    "unapplied dirt on collapsed components does not repeatedly request redraw",
    "[artboard][redraw]")
{
  TransformScene scene;
  scene.node->collapse(true);
  scene.artboard.advance(frameTime);
  scene.artboard.draw(&scene.renderer);
  scene.node->x(scene.node->x() + 20);
  scene.artboard.draw(&scene.renderer);

  for (int i = 0; i < 3; ++i)
  {
    scene.artboard.advance(frameTime);
    CHECK_FALSE(scene.artboard.didChange());
  }

  scene.node->collapse(false);
  scene.artboard.advance(frameTime);
  CHECK(scene.artboard.didChange());
  scene.artboard.draw(&scene.renderer);
  scene.artboard.advance(frameTime);
  CHECK_FALSE(scene.artboard.didChange());
}
