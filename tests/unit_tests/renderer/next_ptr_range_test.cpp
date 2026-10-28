/*
 * Copyright 2026 Rive
 */

#include <catch.hpp>
#include "rive/renderer/next_ptr_range.hpp"
#include "rive/renderer/trivial_block_allocator.hpp"
#include <vector>

using namespace rive;

namespace
{
// Mutable "next", like DrawBatch::ElementRange.
struct Node
{
  int value;
  Node* next = nullptr;
};

// Const "next", like DrawBatch.
struct ConstLinkedNode
{
  ConstLinkedNode(int value_) : value(value_) {}
  int value;
  const ConstLinkedNode* next = nullptr;
};

template <typename Range> std::vector<int> values(const Range& range)
{
  std::vector<int> values;
  for (const auto& node : range)
  {
    values.push_back(node.value);
  }
  return values;
}
} // namespace

TEST_CASE("next_ptr_iterator: empty", "[next_ptr_iterator]")
{
  NextPtrRange<Node> range(nullptr);
  CHECK(range.begin() == range.end());
  CHECK(values(range).empty());
}

TEST_CASE("next_ptr_iterator: walks every node in order", "[next_ptr_iterator]")
{
  Node c{3}, b{2, &c}, a{1, &b};
  CHECK(values(NextPtrRange<Node>(&a)) == std::vector<int>{1, 2, 3});
  CHECK(values(NextPtrRange<const Node>(&a)) == std::vector<int>{1, 2, 3});
}

TEST_CASE(
    "next_ptr_iterator: stops before a non-null end",
    "[next_ptr_iterator]")
{
  Node d{4}, c{3, &d}, b{2, &c}, a{1, &b};
  CHECK(values(NextPtrRange<Node>(&a, &c)) == std::vector<int>{1, 2});
  CHECK(values(NextPtrRange<Node>(&b, &c)) == std::vector<int>{2});
  CHECK(values(NextPtrRange<Node>(&c, &c)).empty());
}

TEST_CASE("next_ptr_iterator: const next pointers", "[next_ptr_iterator]")
{
  ConstLinkedNode b(2), a(1);
  a.next = &b;
  CHECK(
      values(NextPtrRange<const ConstLinkedNode>(&a)) ==
      std::vector<int>{1, 2});
}

TEST_CASE(
    "next_ptr_iterator: writes through a mutable range",
    "[next_ptr_iterator]")
{
  Node c{3}, b{2, &c}, a{1, &b};
  for (Node& node : NextPtrRange<Node>(&a))
  {
    node.value *= 10;
  }
  CHECK(values(NextPtrRange<Node>(&a)) == std::vector<int>{10, 20, 30});
}

TEST_CASE("next_ptr_iterator: BlockAllocatedLinkedList", "[next_ptr_iterator]")
{
  TrivialBlockAllocator allocator(128);
  BlockAllocatedLinkedList<ConstLinkedNode> list;
  CHECK(values(list).empty());
  for (int i = 1; i <= 3; ++i)
  {
    list.emplace_back(allocator, i);
  }
  CHECK(values(list) == std::vector<int>{1, 2, 3});
  const auto& constList = list;
  CHECK(values(constList) == std::vector<int>{1, 2, 3});
}
