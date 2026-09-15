// Binds authored on a file level view model instance's values. The asset is
// built by rive-cli from packages/rml/tests/rmls/vm_instance_value_bind.rml.
#include "rive/artboard_component_list.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/file.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>

using namespace rive;

static ViewModelInstanceBoolean* boolean(ViewModelInstance* instance,
                                         const char* name)
{
    return instance->propertyValue(name)->as<ViewModelInstanceBoolean>();
}

static ViewModelInstanceNumber* number(ViewModelInstance* instance,
                                       const char* name)
{
    return instance->propertyValue(name)->as<ViewModelInstanceNumber>();
}

static bool owns(ViewModelInstance* instance, Core* value)
{
    for (auto& propertyValue : instance->propertyValues())
    {
        if (propertyValue.get() == value)
        {
            return true;
        }
    }
    return false;
}

TEST_CASE("instance value binds import onto the instance and clone with it",
          "[instance_value_bind]")
{
    auto file = ReadRiveFile("assets/instance_value_binds.riv");
    // The file level instance has no property names yet, so match by owner.
    auto source = file->viewModel("Row")->instance("Flag Row");
    REQUIRE(source != nullptr);
    REQUIRE(source->valueDataBinds().size() == 1);
    auto sourceBind = *source->valueDataBinds().begin();
    REQUIRE(sourceBind->target()->is<ViewModelInstanceBoolean>());
    REQUIRE(owns(source, sourceBind->target()));
    REQUIRE(sourceBind->toSource());
    REQUIRE(sourceBind->toTarget());

    auto copy = file->createViewModelInstance("Row", "Flag Row");
    REQUIRE(copy != nullptr);
    REQUIRE(copy->valueDataBinds().size() == 1);
    auto copyBind = *copy->valueDataBinds().begin();
    REQUIRE(copyBind != sourceBind);
    REQUIRE(copyBind->target() == boolean(copy.get(), "on"));

    auto converted = file->viewModel("Row")->instance("Level Row");
    REQUIRE(converted->valueDataBinds().size() == 1);
    REQUIRE((*converted->valueDataBinds().begin())->converter() != nullptr);
}

TEST_CASE("list item instance value binds follow the host both ways",
          "[instance_value_bind]")
{
    auto file = ReadRiveFile("assets/instance_value_binds.riv");
    auto artboard = file->artboard("Host")->instance();
    auto stateMachine = artboard->stateMachineAt(0);
    auto host = file->createDefaultViewModelInstance(artboard.get());
    auto flag = boolean(host.get(), "flag");
    auto level = number(host.get(), "level");
    flag->propertyValue(true);

    stateMachine->bindViewModelInstance(host);
    stateMachine->advanceAndApply(0.0f);

    auto list = artboard->find<ArtboardComponentList>("Rows");
    REQUIRE(list != nullptr);
    REQUIRE(list->artboardCount() == 2);
    auto flagRow = list->listItem(0)->viewModelInstance();
    auto on = boolean(flagRow.get(), "on");
    // The host wins the initial reconcile even though the row authored false.
    CHECK(on->propertyValue() == true);
    CHECK(flag->propertyValue() == true);

    flag->propertyValue(false);
    stateMachine->advanceAndApply(0.016f);
    CHECK(on->propertyValue() == false);

    // A row side write, what its listener does, reaches the host.
    on->propertyValue(true);
    stateMachine->advanceAndApply(0.016f);
    CHECK(flag->propertyValue() == true);

    auto amount =
        number(list->listItem(1)->viewModelInstance().get(), "amount");
    CHECK(amount->propertyValue() == 20.0f);
    level->propertyValue(4.0f);
    stateMachine->advanceAndApply(0.016f);
    CHECK(amount->propertyValue() == 8.0f);
}

TEST_CASE("rebinding an artboard swaps its instance value binds",
          "[instance_value_bind]")
{
    auto file = ReadRiveFile("assets/instance_value_binds.riv");
    auto artboard = file->artboard("Row")->instance();
    auto stateMachine = artboard->stateMachineAt(0);
    auto first = file->createViewModelInstance("Row", "Flag Row");
    auto second = file->createViewModelInstance("Row", "Flag Row");

    stateMachine->bindViewModelInstance(first);
    stateMachine->advanceAndApply(0.0f);
    auto countInstanceBinds = [&]() {
        size_t count = 0;
        for (auto dataBind : artboard->dataBinds())
        {
            if (dataBind->isInstanceValueBind())
            {
                count++;
            }
        }
        return count;
    };
    REQUIRE(countInstanceBinds() == 1);

    stateMachine->bindViewModelInstance(second);
    stateMachine->advanceAndApply(0.016f);
    REQUIRE(countInstanceBinds() == 1);
    for (auto dataBind : artboard->dataBinds())
    {
        if (dataBind->isInstanceValueBind())
        {
            CHECK(dataBind->target() == boolean(second.get(), "on"));
        }
    }

    stateMachine->bindViewModelInstance(nullptr);
    REQUIRE(countInstanceBinds() == 0);
}

TEST_CASE("removing a bound value drops the binds aimed at it",
          "[instance_value_bind]")
{
    auto file = ReadRiveFile("assets/instance_value_binds.riv");
    auto artboard = file->artboard("Row")->instance();
    auto stateMachine = artboard->stateMachineAt(0);
    auto instance = file->createViewModelInstance("Row", "Flag Row");
    stateMachine->bindViewModelInstance(instance);
    stateMachine->advanceAndApply(0.0f);
    auto countInstanceBinds = [&]() {
        size_t count = 0;
        for (auto dataBind : artboard->dataBinds())
        {
            count += dataBind->isInstanceValueBind() ? 1 : 0;
        }
        return count;
    };
    REQUIRE(countInstanceBinds() == 1);

    auto on = boolean(instance.get(), "on");
    REQUIRE(instance->removeValue(on->viewModelPropertyId()));
    CHECK(instance->valueDataBinds().size() == 0);
    CHECK(countInstanceBinds() == 0);
    // Nothing left points at the freed value.
    stateMachine->advanceAndApply(0.016f);
}

TEST_CASE("swapping the context's main instance resyncs instance value binds",
          "[instance_value_bind]")
{
    auto file = ReadRiveFile("assets/instance_value_binds.riv");
    auto artboard = file->artboard("Row")->instance();
    auto stateMachine = artboard->stateMachineAt(0);
    auto first = file->createViewModelInstance("Row", "Flag Row");
    auto second = file->createViewModelInstance("Row", "Flag Row");
    stateMachine->bindViewModelInstance(first);
    stateMachine->advanceAndApply(0.0f);
    auto instanceBindTargets = [&]() {
        std::vector<Core*> targets;
        for (auto dataBind : artboard->dataBinds())
        {
            if (dataBind->isInstanceValueBind())
            {
                targets.push_back(dataBind->target());
            }
        }
        return targets;
    };
    REQUIRE(instanceBindTargets().size() == 1);

    // A direct context mutation, as a host may do, never runs a rebind.
    artboard->dataContext()->setMainViewModelInstance(nullptr);
    CHECK(instanceBindTargets().empty());

    artboard->dataContext()->setMainViewModelInstance(first);
    REQUIRE(instanceBindTargets().size() == 1);
    artboard->dataContext()->removeMainViewModelInstance();
    CHECK(instanceBindTargets().empty());

    artboard->dataContext()->setMainViewModelInstance(second);
    auto targets = instanceBindTargets();
    REQUIRE(targets.size() == 1);
    CHECK(targets[0] == boolean(second.get(), "on"));
}

TEST_CASE("relinking the context's main instance resyncs instance value binds",
          "[instance_value_bind]")
{
    auto file = ReadRiveFile("assets/instance_value_binds.riv");
    auto artboard = file->artboard("Row")->instance();
    auto stateMachine = artboard->stateMachineAt(0);
    auto first = file->createViewModelInstance("Row", "Flag Row");
    auto second = file->createViewModelInstance("Row", "Flag Row");
    stateMachine->bindViewModelInstance(first);
    stateMachine->advanceAndApply(0.0f);

    // The plain setter is what a non-stateful nested artboard relinks with.
    artboard->dataContext()->viewModelInstance(second);
    size_t count = 0;
    for (auto dataBind : artboard->dataBinds())
    {
        if (dataBind->isInstanceValueBind())
        {
            count++;
            CHECK(dataBind->target() == boolean(second.get(), "on"));
        }
    }
    CHECK(count == 1);
}
