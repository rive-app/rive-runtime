#include "rive/artboard.hpp"
#include "rive/data_bind/converters/data_converter_group.hpp"
#include "rive/data_bind/converters/data_converter_group_item.hpp"
#include "rive/data_bind/converters/data_converter_to_string.hpp"
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/data_bind_context.hpp"
#include "rive/data_bind/data_values/data_value_list.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive/file.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <unordered_set>

TEST_CASE(
    "An artboard instance's data binds share their source paths",
    "[data binding]")
{
  size_t shared = 0;
  for (const char* path :
       {"assets/data_binding_test.riv", "assets/data_binding_test_2.riv"})
  {
    auto file = ReadRiveFile(path);
    for (size_t i = 0; i < file->artboardCount(); i++)
    {
      auto source = file->artboard(i);
      std::unordered_set<const std::vector<uint32_t>*> sourcePaths;
      for (auto dataBind : source->dataBinds())
      {
        if (dataBind->is<rive::DataBindContext>() &&
            !dataBind->as<rive::DataBindContext>()->sourcePathIds().empty())
        {
          sourcePaths.insert(
              &dataBind->as<rive::DataBindContext>()->sourcePathIds());
        }
      }
      auto instance = source->instance();
      for (auto dataBind : instance->dataBinds())
      {
        if (!dataBind->is<rive::DataBindContext>())
        {
          continue;
        }
        auto& ids = dataBind->as<rive::DataBindContext>()->sourcePathIds();
        if (ids.empty())
        {
          continue;
        }
        // The very vector the source's bind reads, not a copy of it.
        CHECK(sourcePaths.count(&ids) == 1);
        shared++;
      }
    }
  }
  CHECK(shared > 0);
}

TEST_CASE("A cloned converter group sizes its items once", "[data binding]")
{
  rive::DataConverterToString first, second, third;
  rive::DataConverterGroup group;
  for (auto* converter : {&first, &second, &third})
  {
    auto item = new rive::DataConverterGroupItem();
    item->converter(converter);
    group.addItem(item);
  }
  auto clone = group.clone()->as<rive::DataConverterGroup>();
  CHECK(clone->items().size() == 3);
  // Sized once, not grown an item at a time.
  CHECK(clone->items().capacity() == 3);
  delete clone;
}

namespace
{
class TestContextValue : public rive::DataBindContextValue
{
public:
  using rive::DataBindContextValue::DataBindContextValue;
  rive::DataValue* value() const { return m_dataValue; }
  // Whether [value] sits inside this object rather than in an allocation of
  // its own.
  bool holds(const rive::DataValue* value) const
  {
    auto begin = reinterpret_cast<const char*>(this);
    auto at = reinterpret_cast<const char*>(value);
    return at >= begin && at < begin + sizeof(*this);
  }
};
} // namespace

TEST_CASE(
    "A data bind's context value holds its source's value",
    "[data binding]")
{
  {
    rive::DataBindContext bind;
    auto number = rive::make_rcp<rive::ViewModelInstanceNumber>();
    number->propertyValue(12.0f);
    bind.source(number);
    TestContextValue context(&bind);
    REQUIRE(context.value() != nullptr);
    CHECK(context.holds(context.value()));
    REQUIRE(context.value()->is<rive::DataValueNumber>());
    CHECK(context.value()->as<rive::DataValueNumber>()->value() == 12.0f);
  }
  {
    // The largest values a source makes.
    rive::DataBindContext bind;
    auto string = rive::make_rcp<rive::ViewModelInstanceString>();
    string->propertyValue("a string past the small string size");
    bind.source(string);
    TestContextValue context(&bind);
    REQUIRE(context.value() != nullptr);
    CHECK(context.holds(context.value()));
    REQUIRE(context.value()->is<rive::DataValueString>());
    CHECK(
        context.value()->as<rive::DataValueString>()->value() ==
        "a string past the small string size");
  }
  {
    rive::DataBindContext bind;
    bind.source(rive::make_rcp<rive::ViewModelInstanceList>());
    TestContextValue context(&bind);
    REQUIRE(context.value() != nullptr);
    CHECK(context.holds(context.value()));
    CHECK(context.value()->is<rive::DataValueList>());
  }
}
