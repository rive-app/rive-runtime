#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"

static const char* sizingFiles[] = {
    "assets/data_binding_test.riv",
    "assets/data_binding_test_2.riv",
    "assets/component_list_rtl_row.riv",
    "assets/layout/layout_horizontal.riv",
};

TEST_CASE(
    "An artboard instance sizes its lists like its source",
    "[instancing]")
{
  size_t checked = 0;
  for (const char* path : sizingFiles)
  {
    auto file = ReadRiveFile(path);
    for (size_t i = 0; i < file->artboardCount(); i++)
    {
      auto source = file->artboard(i);
      auto instance = source->instance();
      REQUIRE(instance != nullptr);
      // Sized once from the source, not grown an entry at a time.
      CHECK(instance->drawables().size() == source->drawables().size());
      CHECK(instance->drawables().capacity() == source->drawables().size());
      CHECK(instance->dataBinds().size() <= source->dataBinds().size());
      CHECK(instance->dataBinds().capacity() == source->dataBinds().size());
      checked += source->drawables().size() + source->dataBinds().size();
    }
  }
  // The files have something to size.
  CHECK(checked > 0);
}

TEST_CASE(
    "A state machine instance sizes its data binds like its machine",
    "[instancing]")
{
  size_t checked = 0;
  for (const char* path : sizingFiles)
  {
    auto file = ReadRiveFile(path);
    for (size_t i = 0; i < file->artboardCount(); i++)
    {
      auto source = file->artboard(i);
      auto instance = source->instance();
      for (size_t m = 0; m < source->stateMachineCount(); m++)
      {
        auto machine = source->stateMachine(m);
        auto machineInstance = instance->stateMachineAt(m);
        REQUIRE(machineInstance != nullptr);
        CHECK(
            machineInstance->dataBinds().capacity() ==
            machine->dataBindCount());
        checked += machine->dataBindCount();
      }
    }
  }
  CHECK(checked > 0);
}
