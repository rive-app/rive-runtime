#include "rive/file.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_style_global.hpp"
#include "rive/text/text_style_on_demand.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive_file_reader.hpp"
#include "utils/no_op_renderer.hpp"
#include "catch.hpp"

using namespace rive;

namespace
{
// Component names aren't exported, so find things by structure.
template <typename T> T* firstObject(Artboard* artboard)
{
  for (auto object : artboard->objects())
  {
    if (object != nullptr && object->is<T>())
    {
      return object->as<T>();
    }
  }
  return nullptr;
}

// The first Text with a child of type T.
template <typename T> Text* textWithChild(Artboard* artboard)
{
  for (auto object : artboard->objects())
  {
    if (object == nullptr || !object->is<Text>())
    {
      continue;
    }
    for (auto child : object->as<Text>()->children())
    {
      if (child->is<T>())
      {
        return object->as<Text>();
      }
    }
  }
  return nullptr;
}

int artboardIndex(File* file, const std::string& name)
{
  for (size_t i = 0; i < file->artboardCount(); i++)
  {
    if (file->artboard(i)->name() == name)
    {
      return (int)i;
    }
  }
  return -1;
}

// text_style_global_list.riv (written by rive_core's
// global_text_style_list_test.dart): "Text A" (local style "Local", 12) and
// "Text B" (only a "Heading" handle exposing fontSize 48), both list-driven by
// the main view model's "items" (ItemVM {textContent, textStyle,
// globalTextStyle}), which starts with one item naming "Body". Style
// artboards: "Heading" (fontSize bound to its view model, default 20), "Body"
// (30); "Plain" is not a text style.
struct ListScene
{
  File* file;
  std::unique_ptr<ArtboardInstance> artboard;
  rcp<ViewModelInstance> viewModelInstance;
  ViewModelInstanceList* items = nullptr;
  Text* textA = nullptr;
  Text* textB = nullptr;

  ListScene(File* file) : file(file)
  {
    artboard = file->artboard("Main")->instance();
    viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);
    items = viewModelInstance->propertyValue("items")
                ->as<ViewModelInstanceList>();
    textA = textWithChild<TextStylePaint>(artboard.get());
    textB = textWithChild<TextStyleGlobal>(artboard.get());
    REQUIRE(items != nullptr);
    REQUIRE(textA != nullptr);
    REQUIRE(textB != nullptr);
    advance();
  }

  void advance()
  {
    NoOpRenderer renderer;
    artboard->advance(0.0f);
    artboard->draw(&renderer);
  }

  rcp<ViewModelInstance> addItem(const std::string& styleName)
  {
    auto instance = file->createViewModelInstance("ItemVM");
    REQUIRE(instance != nullptr);
    instance->propertyValue("textContent")
        ->as<ViewModelInstanceString>()
        ->propertyValue("item ");
    instance->propertyValue("textStyle")
        ->as<ViewModelInstanceString>()
        ->propertyValue(styleName);
    auto item = make_rcp<ViewModelInstanceListItem>();
    item->viewModelInstance(instance);
    items->addItem(item);
    return instance;
  }

  static ViewModelInstanceArtboard* global(rcp<ViewModelInstance> instance)
  {
    return instance->propertyValue("globalTextStyle")
        ->as<ViewModelInstanceArtboard>();
  }

  // Advances and draws, then reports the font size [text]'s run [index]
  // paints with.
  float fontSize(Text* text, size_t index)
  {
    advance();
    REQUIRE(index < text->runs().size());
    auto paint = text->runs()[index]->style();
    REQUIRE(paint != nullptr);
    return paint->fontSize();
  }
};
} // namespace

TEST_CASE("List runs resolve global text styles on demand", "[textStyleGlobal]")
{
  auto file = ReadRiveFile("assets/text_style_global_list.riv");
  ListScene scene(file.get());

  // The authored item names "Body": neither Text has a handle for it.
  CHECK(scene.fontSize(scene.textA, 0) == 30.0f);
  CHECK(scene.fontSize(scene.textB, 0) == 30.0f);
  CHECK(scene.textA->onDemandStyleCount() == 1);
  CHECK(scene.textB->onDemandStyleCount() == 1);

  // By file index. Text B's Heading handle wins, with its own value.
  auto heading = scene.addItem("Local");
  ListScene::global(heading)->propertyValue(
      artboardIndex(file.get(), "Heading"));
  CHECK(scene.fontSize(scene.textA, 1) == 20.0f);
  CHECK(scene.fontSize(scene.textB, 1) == 48.0f);
  CHECK(scene.textA->onDemandStyleCount() == 2);
  CHECK(scene.textB->onDemandStyleCount() == 1);

  // By a runtime-set artboard: shares the Text's existing Body style.
  auto body = scene.addItem("Local");
  ListScene::global(body)->asset(file->bindableArtboardNamed("Body"));
  CHECK(scene.fontSize(scene.textA, 2) == 30.0f);
  CHECK(scene.textA->onDemandStyleCount() == 2);
  CHECK(
      scene.textA->runs()[2]->styleReference() ==
      scene.textA->runs()[0]->styleReference());

  // No style, or an artboard that isn't one, falls back to the name, then
  // the first style (Text B's only style is its handle).
  ListScene::global(heading)->propertyValue(-1);
  CHECK(scene.fontSize(scene.textA, 1) == 12.0f);
  CHECK(scene.fontSize(scene.textB, 1) == 48.0f);
  ListScene::global(heading)->propertyValue(artboardIndex(file.get(), "Plain"));
  CHECK(scene.fontSize(scene.textA, 1) == 12.0f);
  ListScene::global(heading)->propertyValue(99999);
  CHECK(scene.fontSize(scene.textA, 1) == 12.0f);

  // Removing items prunes the styles no run uses.
  while (scene.items->listItems().size() > 0)
  {
    scene.items->removeItem(0);
  }
  scene.advance();
  CHECK(scene.textA->runs().empty());
  CHECK(scene.textA->onDemandStyleCount() == 0);
  CHECK(scene.textB->onDemandStyleCount() == 0);
  CHECK(scene.textA->quietState() == AdvancingComponent::QuietState::quiet);
}

TEST_CASE("List runs re-resolve when a handle swaps style", "[textStyleGlobal]")
{
  auto file = ReadRiveFile("assets/text_style_global_list.riv");
  ListScene scene(file.get());
  auto heading = scene.addItem("");
  ListScene::global(heading)->propertyValue(
      artboardIndex(file.get(), "Heading"));
  CHECK(scene.fontSize(scene.textB, 1) == 48.0f);

  // Text B's handle now shows Body: the run still wants Heading, which now
  // comes on demand with its default value.
  auto handle = firstObject<TextStyleGlobal>(scene.artboard.get());
  REQUIRE(handle != nullptr);
  ViewModelInstanceArtboard swap;
  swap.asset(file->bindableArtboardNamed("Body"));
  handle->updateArtboard(&swap);
  CHECK(scene.fontSize(scene.textB, 1) == 20.0f);
  CHECK(scene.textB->runs()[1]->styleReference() != handle);
}

TEST_CASE(
    "Item view models outlive the Text that used them",
    "[textStyleGlobal]")
{
  auto file = ReadRiveFile("assets/text_style_global_list.riv");
  rcp<ViewModelInstance> item;
  {
    ListScene scene(file.get());
    item = scene.addItem("Local");
    ListScene::global(item)->propertyValue(
        artboardIndex(file.get(), "Heading"));
    CHECK(scene.fontSize(scene.textA, 1) == 20.0f);
  }
  // The artboard (its Texts, runs and on-demand styles) is gone; changing
  // the item must not reach them.
  ListScene::global(item)->propertyValue(artboardIndex(file.get(), "Body"));
  item->propertyValue("textStyle")
      ->as<ViewModelInstanceString>()
      ->propertyValue("Other");
}
