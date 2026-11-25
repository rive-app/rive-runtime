#include "rive/artboard.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/file.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>

using namespace rive;

namespace
{
struct ExpectedBind
{
  size_t slot;
  const DataBind* source;
};

// What instance() has always cloned: each object in order (the artboard
// first), and for each, the binds that target it in the source's order.
std::vector<ExpectedBind> expectedBinds(const Artboard* source)
{
  std::vector<ExpectedBind> expected;
  const auto& objects = source->objects();
  for (size_t slot = 0; slot < objects.size(); slot++)
  {
    for (const DataBind* bind : source->dataBinds())
    {
      if (bind->target() == objects[slot])
      {
        expected.push_back({slot, bind});
      }
    }
  }
  return expected;
}

// Checks every artboard in [path]; returns how many binds it compared and
// how many targets carry more than one bind.
void checkFile(const char* path, size_t* compared, size_t* sharedTargets)
{
  auto file = ReadRiveFile(path);
  for (size_t index = 0; index < file->artboardCount(); index++)
  {
    Artboard* source = file->artboard(index);
    REQUIRE(source != nullptr);
    auto expected = expectedBinds(source);
    auto instance = source->instance();
    REQUIRE(instance != nullptr);
    const auto& binds = instance->dataBinds();
    // Binds an instance adds after cloning (none before it is bound) would
    // come after the clones.
    REQUIRE(binds.size() >= expected.size());
    const auto& objects = instance->objects();
    for (size_t i = 0; i < expected.size(); i++)
    {
      const DataBind* clone = binds[i];
      const DataBind* original = expected[i].source;
      CHECK(clone->target() == objects[expected[i].slot]);
      CHECK(clone->propertyKey() == original->propertyKey());
      CHECK(clone->flags() == original->flags());
      CHECK(clone->coreType() == original->coreType());
      CHECK(
          (clone->converter() == nullptr) ==
          (original->converter() == nullptr));
      if (i > 0 && expected[i].slot == expected[i - 1].slot)
      {
        (*sharedTargets)++;
      }
    }
    *compared += expected.size();
  }
}
} // namespace

TEST_CASE(
    "an instance clones its source's binds in the source's order",
    "[data binding]")
{
  size_t compared = 0, sharedTargets = 0;
  // 30 binds on the main artboard: instance() finds them through the
  // sorted lookup.
  checkFile("assets/data_binding_test.riv", &compared, &sharedTargets);
  CHECK(compared >= 30);
  // Sorted lookups where some targets carry several binds, so the order of
  // a target's binds is checked too.
  checkFile("assets/data_viz_demo.riv", &compared, &sharedTargets);
  checkFile(
      "assets/text_follow_path_shape_length.riv",
      &compared,
      &sharedTargets);
  CHECK(sharedTargets >= 12);
  // Few enough binds per artboard for the plain scan.
  checkFile("assets/data_bind_test_cmdq.riv", &compared, &sharedTargets);
  checkFile("assets/collapse_data_binds.riv", &compared, &sharedTargets);
}
