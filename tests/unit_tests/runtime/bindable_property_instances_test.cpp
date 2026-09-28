#include "rive/animation/state_machine_instance_clusters.hpp"
#include <catch.hpp>
#include <set>
#include <vector>

// The table only compares keys, never dereferences them, so addresses inside
// a buffer stand in for BindableProperty objects.
static rive::BindableProperty* fakeProperty(std::vector<char>& buffer,
                                            size_t index)
{
    return reinterpret_cast<rive::BindableProperty*>(buffer.data() +
                                                     index * 16);
}

TEST_CASE("bindable property instances find what was inserted",
          "[statemachine]")
{
    constexpr size_t count = 1000;
    std::vector<char> shared(count * 16 * 2);
    std::vector<char> instances(count * 16);
    rive::BindablePropertyInstances table;
    CHECK(table.find(fakeProperty(shared, 0)) == nullptr);

    // Growing past its initial size several times keeps every entry.
    for (size_t i = 0; i < count; i++)
    {
        table.insert(fakeProperty(shared, i * 2), fakeProperty(instances, i));
    }
    for (size_t i = 0; i < count; i++)
    {
        CHECK(table.find(fakeProperty(shared, i * 2)) ==
              fakeProperty(instances, i));
        // Keys in between were never inserted.
        CHECK(table.find(fakeProperty(shared, i * 2 + 1)) == nullptr);
    }

    std::set<rive::BindableProperty*> visited;
    table.forEachInstance(
        [&](rive::BindableProperty* instance) { visited.insert(instance); });
    CHECK(visited.size() == count);

    table.clear();
    CHECK(table.find(fakeProperty(shared, 0)) == nullptr);
    size_t remaining = 0;
    table.forEachInstance([&](rive::BindableProperty*) { remaining++; });
    CHECK(remaining == 0);
}
