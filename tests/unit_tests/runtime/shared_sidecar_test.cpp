#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/sidecar.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <string>

namespace
{
struct Cluster
{
  std::string text;
};
} // namespace

TEST_CASE("SharedSidecar shares a copy until either side writes", "[sidecar]")
{
  rive::SharedSidecar<Cluster> source;
  CHECK(source.get() == nullptr);
  source.ensureAllocated()->text = "source";

  rive::SharedSidecar<Cluster> copy = source;
  CHECK(copy.get() == source.get());
  CHECK(source.isShared());

  // The writer gets its own cluster; the other side keeps the old value.
  copy.ensureAllocated()->text = "copy";
  CHECK(copy.get() != source.get());
  CHECK(source.get()->text == "source");
  CHECK(copy.get()->text == "copy");
  CHECK(!source.isShared());
  CHECK(!copy.isShared());

  // Writing to the source after sharing again leaves the copy alone too.
  rive::SharedSidecar<Cluster> second;
  second = source;
  source.ensureAllocated()->text = "changed";
  CHECK(second.get()->text == "source");
  CHECK(source.get()->text == "changed");

  // The last holder of a cluster frees it; the others still read it.
  {
    rive::SharedSidecar<Cluster> temporary = second;
    CHECK(second.isShared());
  }
  CHECK(!second.isShared());
  CHECK(second.get()->text == "source");

  rive::SharedSidecar<Cluster> moved = std::move(second);
  CHECK(second.get() == nullptr);
  CHECK(moved.get()->text == "source");
  moved.reset();
  CHECK(moved.get() == nullptr);

  // An empty cluster copies as empty.
  rive::SharedSidecar<Cluster> empty;
  rive::SharedSidecar<Cluster> emptyCopy = empty;
  CHECK(emptyCopy.get() == nullptr);
  CHECK(!emptyCopy.isShared());
}

TEST_CASE(
    "An artboard instance shares its components' names with the source",
    "[sidecar]")
{
  auto file = ReadRiveFile("assets/background_measure.riv");
  auto source = file->artboard();
  auto instance = file->artboardDefault();
  REQUIRE(instance != nullptr);

  auto sourceShape = source->find<rive::Shape>("background");
  auto instanceShape = instance->find<rive::Shape>("background");
  REQUIRE(sourceShape != nullptr);
  REQUIRE(instanceShape != nullptr);
  REQUIRE(sourceShape != instanceShape);
  // Shared, not copied: both read the same string.
  CHECK(&instanceShape->name() == &sourceShape->name());

  auto sourceRun = source->find<rive::TextValueRun>("nameRun");
  auto instanceRun = instance->find<rive::TextValueRun>("nameRun");
  REQUIRE(sourceRun != nullptr);
  REQUIRE(instanceRun != nullptr);
  CHECK(&instanceRun->name() == &sourceRun->name());

  // Renaming the instance's component leaves the source's name.
  instanceShape->name("renamed");
  CHECK(instanceShape->name() == "renamed");
  CHECK(sourceShape->name() == "background");
  CHECK(source->find<rive::Shape>("background") == sourceShape);

  // And renaming the source's leaves an instance made before it.
  auto second = file->artboardDefault();
  sourceRun->name("sourceRenamed");
  CHECK(sourceRun->name() == "sourceRenamed");
  CHECK(instanceRun->name() == "nameRun");
  CHECK(second->find<rive::TextValueRun>("nameRun") != nullptr);

  // The source and its other instances outlive one instance's names.
  instance = nullptr;
  CHECK(second->find<rive::Shape>("background")->name() == "background");
}
