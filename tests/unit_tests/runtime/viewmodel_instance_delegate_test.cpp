#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include <catch.hpp>
#include <functional>
#include <new>

using namespace rive;

namespace
{
struct CountingDelegate final : public ViewModelInstanceValueDelegate
{
  int calls = 0;
  std::function<void()> onChange;
  void valueChanged() override
  {
    calls++;
    if (onChange)
    {
      onChange();
    }
  }
};
} // namespace

TEST_CASE(
    "A delegate removed during a change is not called for it",
    "[viewmodel]")
{
  auto number = make_rcp<ViewModelInstanceNumber>();
  CountingDelegate first;
  CountingDelegate second;
  // Script hosts free a delegate as they remove it.
  first.onChange = [&]() { number->removeDelegate(&second); };
  number->addDelegate(&first);
  number->addDelegate(&second);
  number->propertyValue(1.0f);
  CHECK(first.calls == 1);
  CHECK(second.calls == 0);
}

TEST_CASE(
    "A delegate added at a removed one's address is not called for the "
    "change it was added in",
    "[viewmodel]")
{
  auto number = make_rcp<ViewModelInstanceNumber>();
  CountingDelegate first;
  // Hosts free a delegate as they remove it, so the next one can take its
  // address.
  alignas(CountingDelegate) unsigned char storage[sizeof(CountingDelegate)];
  auto* removed = new (storage) CountingDelegate();
  CountingDelegate* added = nullptr;
  first.onChange = [&]() {
    if (added == nullptr)
    {
      number->removeDelegate(removed);
      removed->~CountingDelegate();
      added = new (storage) CountingDelegate();
      number->addDelegate(added);
    }
  };
  number->addDelegate(&first);
  number->addDelegate(removed);
  number->propertyValue(1.0f);
  REQUIRE(added != nullptr);
  CHECK(added->calls == 0);
  number->propertyValue(2.0f);
  CHECK(added->calls == 1);
  number->removeDelegate(added);
  added->~CountingDelegate();
}

TEST_CASE(
    "A change made by a delegate leaves the delegates being called alone",
    "[viewmodel]")
{
  auto number = make_rcp<ViewModelInstanceNumber>();
  CountingDelegate first;
  CountingDelegate second;
  CountingDelegate late;
  first.onChange = [&]() {
    if (first.calls == 1)
    {
      number->removeDelegate(&second);
      number->addDelegate(&late);
      number->propertyValue(2.0f);
    }
  };
  number->addDelegate(&first);
  number->addDelegate(&second);
  number->propertyValue(1.0f);
  CHECK(second.calls == 0);
  CHECK(late.calls == 0);
  number->propertyValue(3.0f);
  CHECK(first.calls == 2);
  CHECK(late.calls == 1);
}
