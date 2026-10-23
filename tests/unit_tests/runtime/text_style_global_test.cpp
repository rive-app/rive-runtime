#include "rive/core.hpp"
#include "rive/advancing_component.hpp"
#include "rive/artboard.hpp"
#include "rive/artboard_referencer.hpp"
#include "rive/file.hpp"
#include "rive/text/text_style_global.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive_file_reader.hpp"
#include "utils/no_op_renderer.hpp"
#include "catch.hpp"

using namespace rive;

// TextStyleGlobal must be wired into the two hardcoded runtime dispatch
// switches so the runtime instances + advances it like a nested artboard.
TEST_CASE(
    "TextStyleGlobal is an advancing component and artboard referencer",
    "[textStyleGlobal]")
{
  TextStyleGlobal global;

  REQUIRE(AdvancingComponent::from(&global) != nullptr);
  REQUIRE(ArtboardReferencer::from(&global) != nullptr);

  global.artboardId(42);
  REQUIRE(global.referencedArtboardId() == 42);
}

// A handle's (or a stateful nested artboard's) view model instance whose view
// model didn't resolve at import, e.g. in an editor export made before its view
// models were known, still clones with its artboard instead of crashing.
TEST_CASE(
    "A view model instance without a view model clones",
    "[textStyleGlobal]")
{
  auto viewModelInstance = make_rcp<ViewModelInstance>();
  REQUIRE(viewModelInstance->viewModel() == nullptr);
  auto cloned = rcp<ViewModelInstance>(
      viewModelInstance->clone()->as<ViewModelInstance>());
  REQUIRE(cloned->viewModel() == nullptr);
}

// resolvedPaint returns the TextStylePaint contained in the referenced style
// artboard (the same object-iteration used for the instanced artboard).
TEST_CASE(
    "TextStyleGlobal resolvedPaint reads the referenced artboard's paint",
    "[textStyleGlobal]")
{
  // The style artboard owns its objects and deletes them on destruction.
  Artboard styleArtboard;
  auto paint = new TextStylePaint();
  styleArtboard.addObject(paint);

  TextStyleGlobal global;

  // Nothing referenced yet.
  REQUIRE(global.resolvedPaint() == nullptr);

  global.referencedArtboard(&styleArtboard);
  REQUIRE(global.resolvedPaint() == paint);
}

namespace
{
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

// text_style_global_binding.riv (written by rive_core's
// global_text_style_binding_test.dart): a Text whose run paints through a
// TextStyleGlobal handle authored against "Heading" (style default fontSize
// 20, the handle's own instance 48), a "Body" style (fontSize 30), a "Plain"
// artboard that is not a text style, and a main view model {style, n} whose
// text style property is bound to the handle's artboardId.
struct BindingScene
{
  std::unique_ptr<ArtboardInstance> artboard;
  rcp<ViewModelInstance> viewModelInstance;
  ViewModelInstanceArtboard* style = nullptr;
  TextValueRun* run = nullptr;
  TextStyleGlobal* handle = nullptr;

  BindingScene(File* file)
  {
    artboard = file->artboard("Main")->instance();
    viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);
    style = viewModelInstance->propertyValue("style")
                ->as<ViewModelInstanceArtboard>();
    run = firstObject<TextValueRun>(artboard.get());
    handle = firstObject<TextStyleGlobal>(artboard.get());
  }

  // Advances and draws, then reports the font size the run paints with.
  float fontSize()
  {
    NoOpRenderer renderer;
    artboard->advance(0.0f);
    artboard->draw(&renderer);
    auto paint = run->style();
    REQUIRE(paint != nullptr);
    return paint->fontSize();
  }
};
} // namespace

TEST_CASE(
    "A text style property swaps a TextStyleGlobal handle's style",
    "[textStyleGlobal]")
{
  auto file = ReadRiveFile("assets/text_style_global_binding.riv");
  BindingScene scene(file.get());
  REQUIRE(scene.style != nullptr);
  REQUIRE(scene.run != nullptr);
  REQUIRE(scene.handle != nullptr);

  // A text style property exports as a plain artboard property, and the
  // number after it still lines up.
  const uint16_t artboardTypeKey = ViewModelInstanceArtboardBase::typeKey;
  CHECK(scene.style->coreType() == artboardTypeKey);
  auto n = scene.viewModelInstance->propertyValue("n");
  REQUIRE(n != nullptr);
  CHECK(n->as<ViewModelInstanceNumber>()->propertyValue() == 7.0f);

  // The authored style, with the handle's own exposed values.
  CHECK(scene.fontSize() == 48.0f);

  // A runtime-set style gets a default instance of its own view model.
  scene.style->asset(file->bindableArtboardNamed("Body"));
  CHECK(scene.fontSize() == 30.0f);

  // Clearing the property reverts to the authored style.
  scene.style->asset(nullptr);
  CHECK(scene.style->propertyValue() == -1);
  CHECK(scene.fontSize() == 48.0f);

  // Swapping by file index works the same, and coming back to the authored
  // style restores the handle's own exposed values.
  scene.style->propertyValue(artboardIndex(file.get(), "Body"));
  CHECK(scene.fontSize() == 30.0f);
  scene.style->propertyValue(artboardIndex(file.get(), "Heading"));
  CHECK(scene.fontSize() == 48.0f);

  // An artboard that isn't a text style keeps the current style.
  scene.style->propertyValue(artboardIndex(file.get(), "Plain"));
  CHECK(scene.fontSize() == 48.0f);
  scene.style->asset(file->bindableArtboardNamed("Body"));
  CHECK(scene.fontSize() == 30.0f);
  scene.style->asset(file->bindableArtboardNamed("Plain"));
  CHECK(scene.fontSize() == 30.0f);
}

TEST_CASE("Text style bindings are per host instance", "[textStyleGlobal]")
{
  auto file = ReadRiveFile("assets/text_style_global_binding.riv");
  BindingScene first(file.get());
  BindingScene second(file.get());

  first.style->asset(file->bindableArtboardNamed("Body"));
  CHECK(first.fontSize() == 30.0f);
  CHECK(second.fontSize() == 48.0f);

  // The file's own artboard is never swapped and owns no style instance.
  auto source = firstObject<TextStyleGlobal>(file->artboard("Main"));
  REQUIRE(source != nullptr);
  CHECK(source->artboardInstance() == nullptr);
}
