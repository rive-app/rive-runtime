#include <rive/viewmodel/viewmodel_instance_symbol_list_index.hpp>
#include <rive/viewmodel/runtime/viewmodel_instance_list_index_runtime.hpp>
#include <rive/data_bind/data_values/data_type.hpp>
#include <rive/refcnt.hpp>
#include <catch.hpp>

using namespace rive;

// The runtime wrapper reports the data type of the underlying symbol list index
// and reads through to its value. It is a read-only view (no setter).
TEST_CASE("list index runtime reports type and reads value", "[data binding]")
{
    // Own the initial reference here; the runtime takes its own ref on
    // construction and releases it on destruction. Reverse-order destruction
    // (runtime before `index`) then drops the refcount to zero without leaking.
    auto index = make_rcp<ViewModelInstanceSymbolListIndex>();
    index->propertyValue(3);

    ViewModelInstanceListIndexRuntime runtime(index.get());

    CHECK(runtime.dataType() == DataType::symbolListIndex);
    CHECK(runtime.value() == 3);

    index->propertyValue(7);
    CHECK(runtime.value() == 7);
}

// Changing the underlying value dirties the wrapper so
// hasChanged()/flushChanges report it, matching the behavior of the other value
// runtimes.
TEST_CASE("list index runtime reports value changes", "[data binding]")
{
    auto index = make_rcp<ViewModelInstanceSymbolListIndex>();
    ViewModelInstanceListIndexRuntime runtime(index.get());

    // No change yet.
    CHECK_FALSE(runtime.hasChanged());
    CHECK_FALSE(runtime.flushChanges());

    // A new value marks the wrapper dirty.
    index->propertyValue(1);
    CHECK(runtime.hasChanged());

    // flushChanges consumes the change and clears the flag.
    CHECK(runtime.flushChanges());
    CHECK_FALSE(runtime.hasChanged());
    CHECK_FALSE(runtime.flushChanges());

    // Setting the same value is a no-op and does not report a change.
    index->propertyValue(1);
    CHECK_FALSE(runtime.hasChanged());

    // A different value reports again.
    index->propertyValue(2);
    CHECK(runtime.hasChanged());
}
