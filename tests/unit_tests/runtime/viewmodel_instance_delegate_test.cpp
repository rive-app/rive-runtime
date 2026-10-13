#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include <catch.hpp>
#include <functional>

using namespace rive;

namespace
{
struct CountingDelegate : public ViewModelInstanceValueDelegate
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

TEST_CASE("A delegate removed during a change is not called for it",
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

TEST_CASE("A change made by a delegate leaves the delegates being called alone",
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
