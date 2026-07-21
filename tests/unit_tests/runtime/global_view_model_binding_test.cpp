#include "rive/file.hpp"
#include "rive/artboard.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/view_model_type.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <vector>
#include <string>

using namespace rive;

namespace
{
std::vector<std::string> boundNames(const DataContext* dc)
{
    std::vector<std::string> names;
    if (dc == nullptr)
    {
        return names;
    }
    for (auto& vmi : dc->viewModelInstances())
    {
        auto vm = vmi != nullptr ? vmi->viewModel() : nullptr;
        names.push_back(vm != nullptr ? vm->name() : "");
    }
    return names;
}
} // namespace

// The file's global view model names are enumerable, in file order.
TEST_CASE("File::globalViewModelNames lists globals in file order",
          "[viewmodel]")
{
    auto file = ReadRiveFile("assets/global_variables_test.riv");
    auto names = file->globalViewModelNames();
    REQUIRE_FALSE(names.empty());
    // Every listed name resolves to a global view model.
    for (auto& name : names)
    {
        auto vm = file->viewModel(name);
        REQUIRE(vm != nullptr);
        REQUIRE(static_cast<ViewModelType>(vm->viewModelType()) ==
                ViewModelType::global);
    }
}

// Instancing no longer auto-creates globals — the data context starts empty.
TEST_CASE("artboard instancing does not auto-create globals", "[viewmodel]")
{
    auto file = ReadRiveFile("assets/global_variables_test.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->dataContext() == nullptr);
}

// Getter is null-until-created; a global becomes readable only after it is set.
TEST_CASE("global getter is null until set", "[viewmodel]")
{
    auto file = ReadRiveFile("assets/global_variables_test.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    auto names = file->globalViewModelNames();
    REQUIRE_FALSE(names.empty());
    const std::string target = names[0];

    REQUIRE(artboard->globalViewModelInstance(target) == nullptr);

    auto instance =
        file->createDefaultViewModelInstance(file->viewModel(target));
    REQUIRE(instance != nullptr);
    REQUIRE(artboard->setGlobalViewModelInstance(target, instance));
    REQUIRE(artboard->globalViewModelInstance(target) == instance);

    // A non-global name is rejected and stays null.
    REQUIRE_FALSE(
        artboard->setGlobalViewModelInstance("not-a-global", instance));
    REQUIRE(artboard->globalViewModelInstance("not-a-global") == nullptr);
}

// set* mutates the data context without applying; order is [main, globals...].
TEST_CASE("set without bind mutates order; bind applies", "[viewmodel]")
{
    auto file = ReadRiveFile("assets/global_variables_test.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    auto globalNames = file->globalViewModelNames();
    REQUIRE_FALSE(globalNames.empty());

    auto mainVmi = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(mainVmi != nullptr);
    auto mainVm = mainVmi->viewModel();
    REQUIRE(mainVm != nullptr);
    REQUIRE(static_cast<ViewModelType>(mainVm->viewModelType()) !=
            ViewModelType::global);

    // Batch: set main + each global, then a single bind().
    artboard->setViewModelInstance(mainVmi);
    for (auto& name : globalNames)
    {
        auto g = file->createDefaultViewModelInstance(file->viewModel(name));
        REQUIRE(artboard->setGlobalViewModelInstance(name, g));
    }

    // The data context already reflects the sets (getter reads pre-bind).
    std::vector<std::string> expected;
    expected.push_back(mainVm->name());
    for (auto& n : globalNames)
    {
        expected.push_back(n);
    }
    REQUIRE(boundNames(artboard->dataContext().get()) == expected);

    artboard->bind();
    // Order is unchanged after applying.
    REQUIRE(boundNames(artboard->dataContext().get()) == expected);
}

// Globals set out of order still land in file-definition order.
TEST_CASE("globals are ordered by file definition regardless of set order",
          "[viewmodel]")
{
    auto file = ReadRiveFile("assets/global_variables_test.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    auto globalNames = file->globalViewModelNames();
    // Need at least two globals to observe ordering.
    REQUIRE(globalNames.size() >= 2);

    // Set them in reverse file order.
    for (auto it = globalNames.rbegin(); it != globalNames.rend(); ++it)
    {
        auto g = file->createDefaultViewModelInstance(file->viewModel(*it));
        REQUIRE(artboard->setGlobalViewModelInstance(*it, g));
    }

    // The data context still lists them in file order.
    REQUIRE(boundNames(artboard->dataContext().get()) == globalNames);

    // Setting a main afterwards keeps globals ordered, main first.
    auto mainVmi = file->createDefaultViewModelInstance(artboard.get());
    if (mainVmi != nullptr &&
        static_cast<ViewModelType>(mainVmi->viewModel()->viewModelType()) !=
            ViewModelType::global)
    {
        artboard->setViewModelInstance(mainVmi);
        std::vector<std::string> expected;
        expected.push_back(mainVmi->viewModel()->name());
        for (auto& n : globalNames)
        {
            expected.push_back(n);
        }
        REQUIRE(boundNames(artboard->dataContext().get()) == expected);
    }
}

// bind() self-completes: binding only a main fills every global slot on the
// fly.
TEST_CASE("bind completes missing global slots on the fly", "[viewmodel]")
{
    auto file = ReadRiveFile("assets/global_variables_test.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    auto globalNames = file->globalViewModelNames();
    REQUIRE_FALSE(globalNames.empty());

    // Bind only a main — no globals set explicitly.
    auto mainVmi = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(mainVmi != nullptr);
    stateMachine->bindViewModelInstance(mainVmi);

    // Every global slot has been completed by bind().
    for (auto& name : globalNames)
    {
        REQUIRE(stateMachine->globalViewModelInstance(name) != nullptr);
    }
    auto dc = stateMachine->dataContext();
    REQUIRE(dc != nullptr);
    // [main, globals...] — main plus one instance per global slot.
    REQUIRE(boundNames(dc.get()).size() == globalNames.size() + 1);
}

// A slot is addressed by its name (file index), not by the instance's own view
// model, so an instance of a different view model can be placed on a slot.
TEST_CASE("slot accepts a different view model instance (override)",
          "[viewmodel]")
{
    auto file = ReadRiveFile("assets/global_variables_test.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    auto globalNames = file->globalViewModelNames();
    REQUIRE(globalNames.size() >= 2);

    const std::string slotA = globalNames[0];
    const std::string vmB = globalNames[1];

    // An instance of view model B, placed onto slot A.
    auto override = file->createDefaultViewModelInstance(file->viewModel(vmB));
    REQUIRE(override != nullptr);
    REQUIRE(override->viewModel()->name() == vmB);

    // Previously rejected (name != instance's VM); now accepted.
    REQUIRE(artboard->setGlobalViewModelInstance(slotA, override));

    // Slot A resolves to the B-typed instance (addressed by slot, not by VM).
    REQUIRE(artboard->globalViewModelInstance(slotA) == override);
    // Occupancy is keyed by slot: B's own slot stays empty — the instance's own
    // viewModelId does not place it into B's slot.
    REQUIRE(artboard->globalViewModelInstance(vmB) == nullptr);
}

// The state machine path exposes the same set/bind + get-by-name behavior.
TEST_CASE("state machine set/bind and get by name", "[viewmodel]")
{
    auto file = ReadRiveFile("assets/global_variables_test.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    auto globalNames = file->globalViewModelNames();
    REQUIRE_FALSE(globalNames.empty());

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    // Null until set.
    REQUIRE(stateMachine->globalViewModelInstance(globalNames[0]) == nullptr);

    auto mainVmi = file->createDefaultViewModelInstance(artboard.get());
    stateMachine->setViewModelInstance(mainVmi);
    for (auto& name : globalNames)
    {
        auto g = file->createDefaultViewModelInstance(file->viewModel(name));
        REQUIRE(stateMachine->setGlobalViewModelInstance(name, g));
    }
    stateMachine->bind();

    auto dc = stateMachine->dataContext();
    REQUIRE(dc != nullptr);
    auto names = boundNames(dc.get());
    REQUIRE(names.size() == globalNames.size() + 1);
    REQUIRE(names[0] == mainVmi->viewModel()->name());

    auto fetched = stateMachine->globalViewModelInstance(globalNames[0]);
    REQUIRE(fetched != nullptr);
    REQUIRE(fetched->viewModel()->name() == globalNames[0]);

    // Replacing by name preserves position.
    auto custom =
        file->createDefaultViewModelInstance(file->viewModel(globalNames[0]));
    REQUIRE(stateMachine->setGlobalViewModelInstance(globalNames[0], custom));
    REQUIRE(stateMachine->globalViewModelInstance(globalNames[0]) == custom);
    REQUIRE(boundNames(stateMachine->dataContext().get()) == names);
}

// A slot only exists for global view models: naming a real but non-global view
// model is rejected and nothing is slotted.
TEST_CASE("setGlobalViewModelInstance rejects a non-global name", "[viewmodel]")
{
    auto file = ReadRiveFile("assets/global_variables_test.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    // Find a real view model that is not a global.
    std::string nonGlobal;
    for (size_t i = 0; i < file->viewModelCount(); i++)
    {
        auto vm = file->viewModel(i);
        if (vm != nullptr && static_cast<ViewModelType>(vm->viewModelType()) !=
                                 ViewModelType::global)
        {
            nonGlobal = vm->name();
            break;
        }
    }
    REQUIRE_FALSE(nonGlobal.empty());

    auto instance =
        file->createDefaultViewModelInstance(file->viewModel(nonGlobal));
    REQUIRE(instance != nullptr);

    // Rejected on both the artboard and the state machine paths; no slot
    // filled.
    REQUIRE_FALSE(artboard->setGlobalViewModelInstance(nonGlobal, instance));
    REQUIRE(artboard->globalViewModelInstance(nonGlobal) == nullptr);
    REQUIRE_FALSE(
        stateMachine->setGlobalViewModelInstance(nonGlobal, instance));
    REQUIRE(stateMachine->globalViewModelInstance(nonGlobal) == nullptr);
}

// bind() with no context set no longer no-ops: it creates a data context and
// completes the main plus every global slot on the fly.
TEST_CASE("bind creates a data context when none is set", "[viewmodel]")
{
    auto file = ReadRiveFile("assets/global_variables_test.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    auto globalNames = file->globalViewModelNames();
    REQUIRE_FALSE(globalNames.empty());

    // Nothing has been set: the state machine has no data context.
    REQUIRE(stateMachine->dataContext() == nullptr);

    stateMachine->bind();

    // A context now exists, holding a main plus one instance per global slot.
    auto dc = stateMachine->dataContext();
    REQUIRE(dc != nullptr);
    REQUIRE(boundNames(dc.get()).size() == globalNames.size() + 1);
    for (auto& name : globalNames)
    {
        REQUIRE(stateMachine->globalViewModelInstance(name) != nullptr);
    }
}

// A null instance empties a previously-set global slot, leaving the others
// untouched.
TEST_CASE("setGlobalViewModelInstance with null empties the slot",
          "[viewmodel]")
{
    auto file = ReadRiveFile("assets/global_variables_test.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    auto globalNames = file->globalViewModelNames();
    // Need at least two globals to prove the others are untouched.
    REQUIRE(globalNames.size() >= 2);

    for (auto& name : globalNames)
    {
        auto g = file->createDefaultViewModelInstance(file->viewModel(name));
        REQUIRE(stateMachine->setGlobalViewModelInstance(name, g));
    }
    REQUIRE(stateMachine->globalViewModelInstance(globalNames[0]) != nullptr);
    REQUIRE(boundNames(stateMachine->dataContext().get()).size() ==
            globalNames.size());

    // Empty the first slot with a null instance.
    REQUIRE(stateMachine->setGlobalViewModelInstance(globalNames[0], nullptr));

    // That slot now reads as unset; every other slot is untouched.
    REQUIRE(stateMachine->globalViewModelInstance(globalNames[0]) == nullptr);
    for (size_t i = 1; i < globalNames.size(); i++)
    {
        REQUIRE(stateMachine->globalViewModelInstance(globalNames[i]) !=
                nullptr);
    }
    REQUIRE(boundNames(stateMachine->dataContext().get()).size() ==
            globalNames.size() - 1);
}

// bind() self-completes the main too: setting only a global (no main) fills a
// main view model instance on the fly when bind() runs.
TEST_CASE("bind adds a main when only a global is set", "[viewmodel]")
{
    auto file = ReadRiveFile("assets/global_variables_test.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    auto globalNames = file->globalViewModelNames();
    REQUIRE_FALSE(globalNames.empty());

    // Set a single global, no main.
    auto g =
        file->createDefaultViewModelInstance(file->viewModel(globalNames[0]));
    REQUIRE(g != nullptr);
    REQUIRE(stateMachine->setGlobalViewModelInstance(globalNames[0], g));

    // Pre-bind: a context exists but has no main instance.
    auto dc = stateMachine->dataContext();
    REQUIRE(dc != nullptr);
    REQUIRE(dc->mainViewModelInstance() == nullptr);

    stateMachine->bind();

    // bind() completed the main; it leads the [main, globals...] order.
    REQUIRE(dc->mainViewModelInstance() != nullptr);
    auto names = boundNames(dc.get());
    REQUIRE(names.size() == globalNames.size() + 1);
    REQUIRE(names[0] == dc->mainViewModelInstance()->viewModel()->name());
}

// Clearing on a fresh state machine is a no-op: with no context there is
// nothing to empty, so none is allocated — but name validation still applies.
TEST_CASE("setGlobalViewModelInstance null on empty context is a no-op",
          "[viewmodel]")
{
    auto file = ReadRiveFile("assets/global_variables_test.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    auto globalNames = file->globalViewModelNames();
    REQUIRE_FALSE(globalNames.empty());

    REQUIRE(stateMachine->dataContext() == nullptr);

    // Clearing an already-empty slot succeeds without allocating a context.
    REQUIRE(stateMachine->setGlobalViewModelInstance(globalNames[0], nullptr));
    REQUIRE(stateMachine->dataContext() == nullptr);

    // A non-global name is still rejected, even with a null instance.
    REQUIRE_FALSE(
        stateMachine->setGlobalViewModelInstance("not-a-global", nullptr));
}

// The artboard path empties a previously-set global slot on a null instance,
// leaving the others untouched — mirroring the state machine behavior.
TEST_CASE("artboard setGlobalViewModelInstance with null empties the slot",
          "[viewmodel]")
{
    auto file = ReadRiveFile("assets/global_variables_test.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    auto globalNames = file->globalViewModelNames();
    // Need at least two globals to prove the others are untouched.
    REQUIRE(globalNames.size() >= 2);

    for (auto& name : globalNames)
    {
        auto g = file->createDefaultViewModelInstance(file->viewModel(name));
        REQUIRE(artboard->setGlobalViewModelInstance(name, g));
    }
    REQUIRE(artboard->globalViewModelInstance(globalNames[0]) != nullptr);
    REQUIRE(boundNames(artboard->dataContext().get()).size() ==
            globalNames.size());

    // Empty the first slot with a null instance.
    REQUIRE(artboard->setGlobalViewModelInstance(globalNames[0], nullptr));

    // That slot now reads as unset; every other slot is untouched.
    REQUIRE(artboard->globalViewModelInstance(globalNames[0]) == nullptr);
    for (size_t i = 1; i < globalNames.size(); i++)
    {
        REQUIRE(artboard->globalViewModelInstance(globalNames[i]) != nullptr);
    }
    REQUIRE(boundNames(artboard->dataContext().get()).size() ==
            globalNames.size() - 1);
}

// The artboard path also no-ops when clearing with no context: none is
// allocated, but name validation still applies.
TEST_CASE("artboard setGlobalViewModelInstance null on empty context no-ops",
          "[viewmodel]")
{
    auto file = ReadRiveFile("assets/global_variables_test.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    auto globalNames = file->globalViewModelNames();
    REQUIRE_FALSE(globalNames.empty());

    REQUIRE(artboard->dataContext() == nullptr);

    // Clearing an already-empty slot succeeds without allocating a context.
    REQUIRE(artboard->setGlobalViewModelInstance(globalNames[0], nullptr));
    REQUIRE(artboard->dataContext() == nullptr);

    // A non-global name is still rejected, even with a null instance.
    REQUIRE_FALSE(
        artboard->setGlobalViewModelInstance("not-a-global", nullptr));
}
