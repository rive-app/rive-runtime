// A NumberToList converter that let go of its items unreferenced each one
// twice: once by hand, and again when its vector of rcps was cleared. Lists
// showing the items still held them, so the last of those to let go freed an
// item already freed.

#include "rive/data_bind/converters/data_converter_number_to_list.hpp"
#include "rive/data_bind/data_values/data_value_list.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/file.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <string>
#include <vector>

namespace
{
uint32_t viewModelIndex(rive::File* file, const std::string& name)
{
  for (uint32_t index = 0; file->viewModel(index) != nullptr; index++)
  {
    if (file->viewModel(index)->name() == name)
    {
      return index;
    }
  }
  FAIL("no view model named " << name);
  return 0;
}
} // namespace

TEST_CASE(
    "A NumberToList converter lets go of its items once when its view model "
    "changes",
    "[number_to_list]")
{
  auto file = ReadRiveFile("assets/recursive_lists.riv");
  rive::DataConverterNumberToList converter;
  converter.file(file.get());
  converter.viewModelId(viewModelIndex(file.get(), "Link"));

  rive::DataValueNumber count(2.0f);
  auto* output = converter.convert(&count, nullptr);
  REQUIRE(output != nullptr);
  REQUIRE(output->is<rive::DataValueList>());
  // A list showing the items holds them too, as a list's rows do.
  std::vector<rive::rcp<rive::ViewModelInstanceListItem>>
      shown = *output->as<rive::DataValueList>()->value();
  REQUIRE(shown.size() == 2);
  // Held by the converter's items, its last output, and the list.
  CHECK(shown[0]->debugging_refcnt() == 3);
  CHECK(shown[1]->debugging_refcnt() == 3);

  converter.viewModelId(viewModelIndex(file.get(), "Chain"));
  // The converter's items let go, once each.
  CHECK(shown[0]->debugging_refcnt() == 2);
  CHECK(shown[1]->debugging_refcnt() == 2);
}
