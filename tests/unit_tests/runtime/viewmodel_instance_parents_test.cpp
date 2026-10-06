#include "rive/viewmodel/viewmodel_instance.hpp"
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
