/*
 * Test: verify ViewModelInstance references don't leak across artboard
 * creation/destruction cycles.
 * Run with: ./test.sh -m "[blinko minimal]"
 */

#include <rive/file.hpp>
#include <rive/artboard.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/viewmodel/viewmodel.hpp>
#include <rive/viewmodel/viewmodel_instance.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstdio>

using namespace rive;

static void printVMDeltas(File* file,
                          const std::vector<int32_t>& baseline,
                          const char* label)
{
    printf("  %s:", label);
    bool any = false;
    for (size_t i = 0; i < file->viewModelCount(); i++)
    {
        int32_t delta = file->viewModel(i)->debugging_refcnt() - baseline[i];
        if (delta != 0)
        {
            printf(" %s=%+d", file->viewModel(i)->name().c_str(), delta);
            any = true;
        }
    }
    if (!any)
        printf(" (none)");
    printf("\n");
}

static bool hasVMLeak(File* file, const std::vector<int32_t>& baseline)
{
    for (size_t i = 0; i < file->viewModelCount(); i++)
    {
        if (file->viewModel(i)->debugging_refcnt() - baseline[i] != 0)
            return true;
    }
    return false;
}

static std::vector<int32_t> captureBaseline(File* file)
{
    std::vector<int32_t> v;
    for (size_t i = 0; i < file->viewModelCount(); i++)
        v.push_back(file->viewModel(i)->debugging_refcnt());
    return v;
}

TEST_CASE("blinko: artboard instance only (no bind, no advance)",
          "[blinko minimal]")
{
    auto file = ReadRiveFile("assets/blinko.riv");
    auto baseline = captureBaseline(file.get());

    {
        auto artboard = file->artboardDefault()->instance();
    }
    printVMDeltas(file.get(), baseline, "After artboard create+destroy");
    CHECK(!hasVMLeak(file.get(), baseline));
}

TEST_CASE("blinko: artboard + VMI (no bind, no advance)",
          "[blinko minimal]")
{
    auto file = ReadRiveFile("assets/blinko.riv");
    auto baseline = captureBaseline(file.get());

    {
        auto artboard = file->artboardDefault()->instance();
        auto vmi = file->createDefaultViewModelInstance(artboard.get());
    }
    printVMDeltas(file.get(), baseline, "After artboard+VMI create+destroy");
    CHECK(!hasVMLeak(file.get(), baseline));
}

TEST_CASE("blinko: artboard + VMI + SM (no bind, no advance)",
          "[blinko minimal]")
{
    auto file = ReadRiveFile("assets/blinko.riv");
    auto baseline = captureBaseline(file.get());

    {
        auto artboard = file->artboardDefault()->instance();
        auto vmi = file->createDefaultViewModelInstance(artboard.get());
        auto machine = artboard->defaultStateMachine();
    }
    printVMDeltas(file.get(), baseline, "After create+destroy (no bind)");
    CHECK(!hasVMLeak(file.get(), baseline));
}

TEST_CASE("blinko: bind only (no advance)",
          "[blinko minimal]")
{
    auto file = ReadRiveFile("assets/blinko.riv");
    auto baseline = captureBaseline(file.get());

    {
        auto artboard = file->artboardDefault()->instance();
        auto vmi = file->createDefaultViewModelInstance(artboard.get());
        auto machine = artboard->defaultStateMachine();
        machine->bindViewModelInstance(vmi);
    }
    printVMDeltas(file.get(), baseline, "After bind+destroy (no advance)");
    CHECK(!hasVMLeak(file.get(), baseline));
}

TEST_CASE("blinko: bind + 1 advance",
          "[blinko minimal]")
{
    auto file = ReadRiveFile("assets/blinko.riv");
    auto baseline = captureBaseline(file.get());

    {
        auto artboard = file->artboardDefault()->instance();
        auto vmi = file->createDefaultViewModelInstance(artboard.get());
        auto machine = artboard->defaultStateMachine();
        machine->bindViewModelInstance(vmi);
        machine->advanceAndApply(0.0f);
    }
    printVMDeltas(file.get(), baseline, "After bind+1advance+destroy");
    CHECK(!hasVMLeak(file.get(), baseline));
}

TEST_CASE("blinko: bind + 30 advances",
          "[blinko minimal]")
{
    auto file = ReadRiveFile("assets/blinko.riv");
    auto baseline = captureBaseline(file.get());

    {
        auto artboard = file->artboardDefault()->instance();
        auto vmi = file->createDefaultViewModelInstance(artboard.get());
        auto machine = artboard->defaultStateMachine();
        machine->bindViewModelInstance(vmi);
        for (int f = 0; f < 30; f++)
            machine->advanceAndApply(0.016f);
    }
    printVMDeltas(file.get(), baseline, "After bind+30advance+destroy");
    CHECK(!hasVMLeak(file.get(), baseline));
}

TEST_CASE("blinko: repeated cycles against same File",
          "[blinko minimal]")
{
    auto file = ReadRiveFile("assets/blinko.riv");
    auto baseline = captureBaseline(file.get());

    for (int cycle = 0; cycle < 5; cycle++)
    {
        {
            auto artboard = file->artboardDefault()->instance();
            auto vmi = file->createDefaultViewModelInstance(artboard.get());
            auto machine = artboard->defaultStateMachine();
            machine->bindViewModelInstance(vmi);
            machine->advanceAndApply(0.0f);
        }
        char label[64];
        snprintf(label, sizeof(label), "After cycle %d", cycle + 1);
        printVMDeltas(file.get(), baseline, label);
    }
    CHECK(!hasVMLeak(file.get(), baseline));
}

TEST_CASE("blinko: destroy order - machine first",
          "[blinko minimal]")
{
    auto file = ReadRiveFile("assets/blinko.riv");
    auto baseline = captureBaseline(file.get());

    auto artboard = file->artboardDefault()->instance();
    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    auto machine = artboard->defaultStateMachine();
    machine->bindViewModelInstance(vmi);
    machine->advanceAndApply(0.0f);

    machine.reset();
    artboard.reset();
    vmi = nullptr;
    printVMDeltas(file.get(), baseline, "After machine->artboard->vmi destroy");
    CHECK(!hasVMLeak(file.get(), baseline));
}

TEST_CASE("blinko: destroy order - artboard first",
          "[blinko minimal]")
{
    auto file = ReadRiveFile("assets/blinko.riv");
    auto baseline = captureBaseline(file.get());

    auto artboard = file->artboardDefault()->instance();
    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    auto machine = artboard->defaultStateMachine();
    machine->bindViewModelInstance(vmi);
    machine->advanceAndApply(0.0f);

    artboard.reset();
    machine.reset();
    vmi = nullptr;
    printVMDeltas(file.get(), baseline, "After artboard->machine->vmi destroy");
    CHECK(!hasVMLeak(file.get(), baseline));
}
