#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include <catch.hpp>
#include <algorithm>

using namespace rive;

TEST_CASE("View model instance keeps its other parents when the first goes",
          "[viewmodel]")
{
    auto child = make_rcp<ViewModelInstance>();
    auto a = make_rcp<ViewModelInstance>();
    auto b = make_rcp<ViewModelInstance>();
    auto c = make_rcp<ViewModelInstance>();
    child->addParent(a.get());
    child->addParent(b.get());
    child->addParent(c.get());
    child->addParent(b.get());
    CHECK(child->parents() ==
          std::vector<ViewModelInstance*>{a.get(), b.get(), c.get()});

    child->removeParent(a.get());
    REQUIRE(child->hasParents());
    auto parents = child->parents();
    std::sort(parents.begin(), parents.end());
    auto expected = std::vector<ViewModelInstance*>{b.get(), c.get()};
    std::sort(expected.begin(), expected.end());
    CHECK(parents == expected);

    child->addParent(a.get());
    child->removeParent(c.get());
    child->removeParent(b.get());
    CHECK(child->parents() == std::vector<ViewModelInstance*>{a.get()});
    child->removeParent(a.get());
    CHECK_FALSE(child->hasParents());
}

TEST_CASE("A list item popped off loses the list's view model as its parent",
          "[viewmodel]")
{
    auto owner = make_rcp<ViewModelInstance>();
    auto list = make_rcp<ViewModelInstanceList>();
    list->parentViewModelInstance(owner.get());
    auto child = make_rcp<ViewModelInstance>();
    auto item = make_rcp<ViewModelInstanceListItem>();
    item->viewModelInstance(child);
    list->addItem(item);
    CHECK(child->parents() == std::vector<ViewModelInstance*>{owner.get()});
    CHECK(list->pop() == item);
    CHECK_FALSE(child->hasParents());
}
