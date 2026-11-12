/*
 * Copyright 2026 Rive
 */

// Deferred file level view model instances (FileImportOptions::
// deferDecoding): import reads each one's record but skips its values, and
// decodes them the first time its view model hands the instance out. These
// tests hold that to an eager import: the same instances once decoded, the
// same frames drawn, values left alone until something needs them, and one
// decode per instance however many threads first use it.

#include "rive/artboard.hpp"
#include "rive/core/vector_binary_writer.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/file.hpp"
#include "rive/generated/artboard_base.hpp"
#include "rive/generated/backboard_base.hpp"
#include "rive/generated/component_base.hpp"
#include "rive/generated/data_bind/converters/data_converter_rounder_base.hpp"
#include "rive/generated/data_bind/data_bind_base.hpp"
#include "rive/generated/layout_component_base.hpp"
#include "rive/generated/viewmodel/viewmodel_base.hpp"
#include "rive/generated/viewmodel/viewmodel_component_base.hpp"
#include "rive/generated/viewmodel/viewmodel_instance_base.hpp"
#include "rive/generated/viewmodel/viewmodel_instance_number_base.hpp"
#include "rive/generated/viewmodel/viewmodel_instance_value_base.hpp"
#include "rive/generated/viewmodel/viewmodel_instance_viewmodel_base.hpp"
#include "rive/generated/viewmodel/viewmodel_property_number_base.hpp"
#include "rive/generated/viewmodel/viewmodel_property_viewmodel_base.hpp"
#include "rive/viewmodel/deferred_viewmodel_instance_body.hpp"
#include "rive/viewmodel/runtime/viewmodel_runtime.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#include "rive/viewmodel/viewmodel_instance_asset.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_color.hpp"
#include "rive/viewmodel/viewmodel_instance_enum.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include "utils/svg_factory.hpp"
#include "lazy_decode_test_helpers.hpp"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

using namespace rive;
using namespace lazy_decode_test;

namespace
{
// Deferring view model instances or not, everything else as the testing
// default says (RIVE_LAZY_DECODING).
rcp<File> import(
    const std::vector<uint8_t>& bytes,
    bool lazy,
    ImportResult* result,
    Factory* factory = &gNoOpFactory)
{
  return File::import(
      bytes,
      factory,
      result,
      (FileAssetLoader*)nullptr,
      nullptr,
      false,
      deferring(deferringByDefault(), deferringByDefault(), lazy));
}

size_t pendingCount(File* file)
{
  size_t count = 0;
  for (size_t i = 0; i < file->viewModelCount(); i++)
  {
    ViewModel* viewModel = file->viewModel(i);
    for (size_t j = 0; j < viewModel->instanceCount(); j++)
    {
      count += viewModel->hasDeferredInstanceBody(j) ? 1 : 0;
    }
  }
  return count;
}

uint32_t outcomeCount(const File* file)
{
  uint32_t count = 0;
  for (size_t i = 0; i < kViewModelInstanceBodySkimCount; i++)
  {
    count += file->deferredViewModelInstanceStats().outcomes[i];
  }
  return count;
}

uint32_t deferredCount(const File* file)
{
  return file->deferredViewModelInstanceStats()
      .outcomes[(size_t)ViewModelInstanceBodySkim::deferred];
}

// Where [value] sits among [instance]'s values, or -1.
int indexOf(ViewModelInstance* instance, const Core* value)
{
  const auto& values = instance->propertyValues();
  for (size_t i = 0; i < values.size(); i++)
  {
    if (values[i].get() == value)
    {
      return (int)i;
    }
  }
  return -1;
}

void requireSameValue(ViewModelInstanceValue* e, ViewModelInstanceValue* l)
{
  REQUIRE(l->coreType() == e->coreType());
  CHECK(l->viewModelPropertyId() == e->viewModelPropertyId());
  CHECK(
      (l->viewModelProperty() == nullptr) ==
      (e->viewModelProperty() == nullptr));
  if (e->is<ViewModelInstanceNumber>())
  {
    CHECK(same(
        l->as<ViewModelInstanceNumber>()->propertyValue(),
        e->as<ViewModelInstanceNumber>()->propertyValue()));
  }
  else if (e->is<ViewModelInstanceString>())
  {
    CHECK(
        l->as<ViewModelInstanceString>()->propertyValue() ==
        e->as<ViewModelInstanceString>()->propertyValue());
  }
  else if (e->is<ViewModelInstanceBoolean>())
  {
    CHECK(
        l->as<ViewModelInstanceBoolean>()->propertyValue() ==
        e->as<ViewModelInstanceBoolean>()->propertyValue());
  }
  else if (e->is<ViewModelInstanceColor>())
  {
    CHECK(
        l->as<ViewModelInstanceColor>()->propertyValue() ==
        e->as<ViewModelInstanceColor>()->propertyValue());
  }
  else if (e->is<ViewModelInstanceEnum>())
  {
    CHECK(
        l->as<ViewModelInstanceEnum>()->propertyValue() ==
        e->as<ViewModelInstanceEnum>()->propertyValue());
  }
  else if (e->is<ViewModelInstanceTrigger>())
  {
    CHECK(
        l->as<ViewModelInstanceTrigger>()->propertyValue() ==
        e->as<ViewModelInstanceTrigger>()->propertyValue());
  }
  else if (e->is<ViewModelInstanceViewModel>())
  {
    CHECK(
        l->as<ViewModelInstanceViewModel>()->propertyValue() ==
        e->as<ViewModelInstanceViewModel>()->propertyValue());
  }
  else if (e->is<ViewModelInstanceArtboard>())
  {
    CHECK(
        l->as<ViewModelInstanceArtboard>()->propertyValue() ==
        e->as<ViewModelInstanceArtboard>()->propertyValue());
  }
  else if (e->is<ViewModelInstanceAsset>())
  {
    CHECK(
        l->as<ViewModelInstanceAsset>()->propertyValue() ==
        e->as<ViewModelInstanceAsset>()->propertyValue());
  }
  else if (e->is<ViewModelInstanceList>())
  {
    auto eItems = e->as<ViewModelInstanceList>()->listItems();
    auto lItems = l->as<ViewModelInstanceList>()->listItems();
    REQUIRE(lItems.size() == eItems.size());
    for (size_t i = 0; i < eItems.size(); i++)
    {
      CHECK(lItems[i]->viewModelId() == eItems[i]->viewModelId());
      CHECK(
          lItems[i]->viewModelInstanceId() == eItems[i]->viewModelInstanceId());
    }
  }
}

// Requires the same instance, decoding [l] by asking its view model for it.
void requireSameInstance(ViewModelInstance* e, ViewModelInstance* l)
{
  CHECK(l->name() == e->name());
  CHECK(l->viewModelId() == e->viewModelId());
  const auto& eValues = e->propertyValues();
  const auto& lValues = l->propertyValues();
  REQUIRE(lValues.size() == eValues.size());
  for (size_t i = 0; i < eValues.size(); i++)
  {
    INFO("value " << i);
    requireSameValue(eValues[i].get(), lValues[i].get());
  }
  // The binds on the values, on the same values.
  const auto& eBinds = e->valueDataBinds().view();
  const auto& lBinds = l->valueDataBinds().view();
  REQUIRE(lBinds.size() == eBinds.size());
  for (size_t i = 0; i < eBinds.size(); i++)
  {
    DataBind* eBind = eBinds[i];
    DataBind* lBind = lBinds[i];
    CHECK(lBind->coreType() == eBind->coreType());
    CHECK(lBind->propertyKey() == eBind->propertyKey());
    CHECK(lBind->flags() == eBind->flags());
    CHECK((lBind->converter() == nullptr) == (eBind->converter() == nullptr));
    CHECK(indexOf(l, lBind->target()) == indexOf(e, eBind->target()));
  }
}
} // namespace

TEST_CASE(
    "lazy view model instances decode to what an eager import reads",
    "[lazy_viewmodel_instance]")
{
  int deferringFiles = 0;
  for (const std::string& path : allAssets())
  {
    INFO(path);
    std::vector<uint8_t> bytes = ReadFile(path.c_str());
    ImportResult eagerResult;
    ImportResult lazyResult;
    auto eager = import(bytes, false, &eagerResult);
    int decodes = File::deferredViewModelInstanceDecodeCount;
    auto lazy = import(bytes, true, &lazyResult);
    // Instances the import itself handed out: an artboard's view model
    // reference names the instance it holds (ViewModelInstanceViewModel).
    int decodedByImport = File::deferredViewModelInstanceDecodeCount - decodes;
    REQUIRE(lazyResult == eagerResult);
    if (eager == nullptr)
    {
      continue;
    }
    REQUIRE(lazy != nullptr);
    REQUIRE(lazy->viewModelCount() == eager->viewModelCount());
    size_t pending = pendingCount(lazy.get());
    CHECK(pending + decodedByImport == deferredCount(lazy.get()));
    deferringFiles += deferredCount(lazy.get()) > 0 ? 1 : 0;
    // Every instance a view model lists is file level, and was either
    // deferred or decoded for a reason.
    size_t fileLevel = 0;
    for (size_t i = 0; i < eager->viewModelCount(); i++)
    {
      for (auto* instance : eager->viewModel(i)->instances())
      {
        CHECK(instance->artboard() == nullptr);
        fileLevel++;
      }
    }
    CHECK(outcomeCount(lazy.get()) == fileLevel);

    for (size_t i = 0; i < eager->viewModelCount(); i++)
    {
      INFO("view model " << i);
      ViewModel* e = eager->viewModel(i);
      ViewModel* l = lazy->viewModel(i);
      CHECK(l->name() == e->name());
      // The instances were all listed at import, deferred or not.
      REQUIRE(l->instanceCount() == e->instanceCount());
      for (size_t j = 0; j < l->instanceCount(); j++)
      {
        INFO("instance " << j);
        requireSameInstance(e->instance(j), l->instance(j));
        CHECK_FALSE(l->hasDeferredInstanceBody(j));
      }
    }
    CHECK(pendingCount(lazy.get()) == 0);
    CHECK(lazy->deferredViewModelInstanceStats().decodeFailures == 0);
  }
  CHECK(deferringFiles > 0);
}

TEST_CASE(
    "lazy view model instances draw the same frames",
    "[lazy_viewmodel_instance]")
{
  int compared = 0;
  for (const std::string& path : allAssets())
  {
    INFO(path);
    std::vector<uint8_t> bytes = ReadFile(path.c_str());
    ImportResult result;
    SVGFactory eagerFactory;
    SVGFactory lazyFactory;
    auto lazy = import(bytes, true, &result, &lazyFactory);
    if (lazy == nullptr || lazy->artboardCount() == 0 ||
        deferredCount(lazy.get()) == 0)
    {
      continue;
    }
    auto eager = import(bytes, false, &result, &eagerFactory);
    REQUIRE(eager != nullptr);
    auto e = svgFrames(eager.get(), 30);
    auto l = svgFrames(lazy.get(), 30);
    REQUIRE(l.size() == e.size());
    for (size_t i = 0; i < e.size(); i++)
    {
      INFO("frame " << i);
      CHECK(l[i] == e[i]);
    }
    compared++;
  }
  CHECK(compared > 0);
}

TEST_CASE(
    "lazy view model instances wait for their first use",
    "[lazy_viewmodel_instance]")
{
  // An instance holding a list of instances of another view model.
  std::vector<uint8_t> bytes = ReadFile("assets/db_health_tracker.riv");
  ImportResult result;
  auto file = import(bytes, true, &result);
  REQUIRE(file != nullptr);
  size_t deferred = pendingCount(file.get());
  REQUIRE(deferred > 1);

  // Names and counts come from the records, the runtime API's too.
  for (size_t i = 0; i < file->viewModelCount(); i++)
  {
    ViewModel* viewModel = file->viewModel(i);
    viewModel->instanceCount();
    ViewModelRuntime* runtime = file->viewModelByIndex(i);
    REQUIRE(runtime != nullptr);
    std::vector<std::string> names = runtime->instanceNames();
    REQUIRE(names.size() == viewModel->instanceCount());
    for (size_t j = 0; j < names.size(); j++)
    {
      CHECK(names[j] == viewModel->instanceName(j));
    }
  }
  CHECK(pendingCount(file.get()) == deferred);

  // Opening the default artboard copies the default instance, which decodes
  // it and whatever it refers to, and only those.
  auto artboard = file->artboardDefault();
  REQUIRE(artboard != nullptr);
  auto instance = file->createDefaultViewModelInstance(artboard.get());
  REQUIRE(instance != nullptr);
  size_t pending = pendingCount(file.get());
  CHECK(pending < deferred);

  // Every other instance decodes when asked for.
  for (size_t i = 0; i < file->viewModelCount(); i++)
  {
    file->viewModel(i)->instances();
  }
  CHECK(pendingCount(file.get()) == 0);
}

TEST_CASE(
    "concurrent first uses decode each instance once",
    "[lazy_viewmodel_instance]")
{
  std::vector<uint8_t> bytes = ReadFile("assets/db_health_tracker.riv");
  ImportResult result;
  auto eager = import(bytes, false, &result);
  REQUIRE(eager != nullptr);
  for (int round = 0; round < 20; round++)
  {
    auto file = import(bytes, true, &result);
    REQUIRE(file != nullptr);
    size_t deferred = pendingCount(file.get());
    REQUIRE(deferred > 0);
    int decodes = File::deferredViewModelInstanceDecodeCount;
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; t++)
    {
      threads.emplace_back([&file, t] {
        // From different ends, so threads meet on the same instances.
        size_t count = file->viewModelCount();
        for (size_t k = 0; k < count; k++)
        {
          size_t i = t % 2 == 0 ? k : count - 1 - k;
          ViewModel* viewModel = file->viewModel(i);
          for (size_t j = 0; j < viewModel->instanceCount(); j++)
          {
            viewModel->instance(j);
          }
          file->createViewModelInstance(viewModel);
        }
      });
    }
    for (auto& thread : threads)
    {
      thread.join();
    }
    CHECK(
        File::deferredViewModelInstanceDecodeCount - decodes == (int)deferred);
    CHECK(pendingCount(file.get()) == 0);
    for (size_t i = 0; i < eager->viewModelCount(); i++)
    {
      for (size_t j = 0; j < eager->viewModel(i)->instanceCount(); j++)
      {
        requireSameInstance(
            eager->viewModel(i)->instance(j),
            file->viewModel(i)->instance(j));
      }
    }
  }
}

// View models list only file level instances, the ones the exporter numbers.
// A nested artboard's stateful instances belong to their artboard, so a
// deferred artboard's decode leaves the lists alone: they read the same
// before and after, and while artboards decode on other threads.
TEST_CASE(
    "view model instance lists don't change as artboards decode",
    "[lazy_viewmodel_instance]")
{
  // What each view model lists, by name, read without decoding anything.
  auto listed = [](const File* file) {
    std::vector<std::vector<std::string>> names(file->viewModelCount());
    for (size_t i = 0; i < file->viewModelCount(); i++)
    {
      const ViewModel* viewModel = file->viewModel(i);
      for (size_t j = 0; j < viewModel->instanceCount(); j++)
      {
        names[i].push_back(viewModel->instanceName(j));
      }
    }
    return names;
  };
  size_t artboardOwned = 0;
  for (const char* path :
       {"assets/component_stateful.riv",
        "assets/component_stateful_vm_instance.riv",
        "assets/stateful_nested.riv",
        "assets/stateful_list_props.riv"})
  {
    INFO(path);
    std::vector<uint8_t> bytes = ReadFile(path);
    ImportResult result;
    auto eager = File::import(bytes, &gNoOpFactory, &result);
    REQUIRE(eager != nullptr);
    auto lazy = File::import(
        bytes,
        &gNoOpFactory,
        &result,
        (FileAssetLoader*)nullptr,
        nullptr,
        false,
        deferring(false, true, true, true));
    REQUIRE(lazy != nullptr);
    const auto expected = listed(eager.get());
    CHECK(listed(lazy.get()) == expected);

    // Half the threads decode every artboard, the others read the lists.
    constexpr int kThreads = 8;
    std::atomic<bool> changed{false};
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; t++)
    {
      threads.emplace_back([&, t] {
        size_t count = lazy->artboardCount();
        for (int round = 0; round < 20; round++)
        {
          if (t % 2 == 0)
          {
            for (size_t i = 0; i < count; i++)
            {
              lazy->artboard((i + t) % count);
            }
          }
          else if (listed(lazy.get()) != expected)
          {
            changed = true;
          }
        }
      });
    }
    for (auto& thread : threads)
    {
      thread.join();
    }
    CHECK_FALSE(changed);
    CHECK(listed(lazy.get()) == expected);

    // The artboards do own instances, which aren't listed.
    for (size_t i = 0; i < lazy->artboardCount(); i++)
    {
      for (Core* object : lazy->artboard(i)->objects())
      {
        if (object != nullptr && object->is<ViewModelInstance>())
        {
          CHECK(object->as<ViewModelInstance>()->viewModel() != nullptr);
          artboardOwned++;
        }
      }
    }
  }
  CHECK(artboardOwned > 0);
}

// View model A has one file level instance, whose number has a bind through
// the file's converter. The second artboard's stateful instance of B names
// it, so decoding that artboard decodes A's instance, its last pending one.
static std::vector<uint8_t> instanceNamedByDeferredArtboard()
{
  std::vector<uint8_t> bytes;
  VectorBinaryWriter writer(&bytes);
  writer.write((const uint8_t*)"RIVE", 4);
  writer.writeVarUint((uint32_t)File::majorVersion);
  writer.writeVarUint((uint32_t)File::minorVersion);
  writer.writeVarUint((uint32_t)0);
  // No table of contents: every key is one the runtime knows.
  writer.writeVarUint((uint32_t)0);
  auto record = [&](uint16_t typeKey) {
    writer.writeVarUint((uint32_t)typeKey);
  };
  auto uintProperty = [&](uint16_t key, uint32_t value) {
    writer.writeVarUint((uint32_t)key);
    writer.writeVarUint(value);
  };
  auto floatProperty = [&](uint16_t key, float value) {
    writer.writeVarUint((uint32_t)key);
    writer.writeFloat(value);
  };
  auto stringProperty = [&](uint16_t key, const std::string& value) {
    writer.writeVarUint((uint32_t)key);
    writer.write(value);
  };
  auto end = [&]() { writer.writeVarUint((uint32_t)0); };
  auto artboard = [&](const char* name) {
    record(ArtboardBase::typeKey);
    stringProperty(ComponentBase::namePropertyKey, name);
    floatProperty(LayoutComponentBase::widthPropertyKey, 100);
    floatProperty(LayoutComponentBase::heightPropertyKey, 100);
    end();
  };
  record(BackboardBase::typeKey);
  end();
  record(DataConverterRounderBase::typeKey);
  end();
  record(ViewModelBase::typeKey);
  stringProperty(ViewModelComponentBase::namePropertyKey, "A");
  end();
  record(ViewModelPropertyNumberBase::typeKey);
  stringProperty(ViewModelComponentBase::namePropertyKey, "n");
  end();
  record(ViewModelBase::typeKey);
  stringProperty(ViewModelComponentBase::namePropertyKey, "B");
  end();
  record(ViewModelPropertyViewModelBase::typeKey);
  stringProperty(ViewModelComponentBase::namePropertyKey, "a");
  uintProperty(
      ViewModelPropertyViewModelBase::viewModelReferenceIdPropertyKey,
      0);
  end();
  record(ViewModelInstanceBase::typeKey);
  stringProperty(ComponentBase::namePropertyKey, "a0");
  uintProperty(ViewModelInstanceBase::viewModelIdPropertyKey, 0);
  end();
  record(ViewModelInstanceNumberBase::typeKey);
  uintProperty(ViewModelInstanceValueBase::viewModelPropertyIdPropertyKey, 0);
  floatProperty(ViewModelInstanceNumberBase::propertyValuePropertyKey, 1.25f);
  end();
  record(DataBindBase::typeKey);
  uintProperty(
      DataBindBase::propertyKeyPropertyKey,
      ViewModelInstanceNumberBase::propertyValuePropertyKey);
  uintProperty(DataBindBase::converterIdPropertyKey, 0);
  end();
  artboard("first");
  artboard("second");
  record(ViewModelInstanceBase::typeKey);
  uintProperty(ComponentBase::parentIdPropertyKey, 0);
  uintProperty(ViewModelInstanceBase::viewModelIdPropertyKey, 1);
  end();
  record(ViewModelInstanceViewModelBase::typeKey);
  uintProperty(ComponentBase::parentIdPropertyKey, 1);
  uintProperty(ViewModelInstanceValueBase::viewModelPropertyIdPropertyKey, 0);
  uintProperty(ViewModelInstanceViewModelBase::propertyValuePropertyKey, 0);
  end();
  return bytes;
}

TEST_CASE(
    "an instance a deferred artboard decodes has its converters when published",
    "[lazy_viewmodel_instance]")
{
  std::vector<uint8_t> bytes = instanceNamedByDeferredArtboard();
  ImportResult result;
  auto eager = File::import(bytes, &gNoOpFactory, &result);
  REQUIRE(eager != nullptr);
  ViewModelInstance* eagerA = eager->viewModel((size_t)0)->instance(0);
  REQUIRE(eagerA->valueDataBinds().size() == 1);
  CHECK(eagerA->valueDataBinds().view()[0]->converter() != nullptr);

  auto lazy = File::import(
      bytes,
      &gNoOpFactory,
      &result,
      (FileAssetLoader*)nullptr,
      nullptr,
      false,
      deferring(false, true, true));
  REQUIRE(lazy != nullptr);
  REQUIRE(lazy->artboardHeader(1)->hasDeferredBody());
  REQUIRE(lazy->viewModel((size_t)0)->hasDeferredInstanceBody(0));
  // Decoding the artboard decodes the instance its stateful instance names,
  // after the import: that read resolves the instance's binds against the
  // file's converters before publishing it, rather than leaving them to the
  // artboard's read (decodeInstanceBody asserts as much).
  int decodes = File::deferredViewModelInstanceDecodeCount;
  Artboard* second = lazy->artboard(1);
  REQUIRE(second != nullptr);
  CHECK(File::deferredViewModelInstanceDecodeCount == decodes + 1);
  CHECK_FALSE(lazy->viewModel((size_t)0)->hasDeferredInstanceBody(0));
  ViewModelInstance* a = lazy->viewModel((size_t)0)->instance(0);
  requireSameInstance(eagerA, a);
  REQUIRE(a->valueDataBinds().size() == 1);
  CHECK(a->valueDataBinds().view()[0]->converter() != nullptr);
  auto reference = second->find<ViewModelInstanceViewModel>();
  CHECK(reference.size() == 1);
  for (auto* value : reference)
  {
    CHECK(value->referenceViewModelInstance().get() == a);
  }
}
