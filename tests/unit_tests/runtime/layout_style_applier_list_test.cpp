#include "rive/layout/layout_data.hpp"
#include <catch.hpp>
#include <vector>

#ifdef WITH_RIVE_LAYOUT
using namespace rive;

static std::vector<LayoutStyleApplier*> contents(
    const LayoutStyleApplierList& list)
{
  return std::vector<LayoutStyleApplier*>(list.begin(), list.end());
}

TEST_CASE("Layout style appliers keep their order and skip repeats", "[layout]")
{
  LayoutStyleApplier a, b, c, d, e;
  LayoutStyleApplierList list;
  CHECK(list.empty());
  CHECK(contents(list).empty());

  list.pushUnique(&a);
  list.pushUnique(&b);
  list.pushUnique(&a);
  list.pushUnique(&c);
  CHECK(list.size() == 3);
  CHECK(contents(list) == std::vector<LayoutStyleApplier*>{&a, &b, &c});

  // Past the inline slots the list moves to the heap, keeping its order and
  // still skipping repeats.
  list.pushUnique(&d);
  list.pushUnique(&b);
  list.pushUnique(&e);
  list.pushUnique(&d);
  CHECK(list.size() == 5);
  CHECK(contents(list) == std::vector<LayoutStyleApplier*>{&a, &b, &c, &d, &e});
}
#endif
