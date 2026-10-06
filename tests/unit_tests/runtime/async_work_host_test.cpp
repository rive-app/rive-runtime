#include "catch.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/async/work_pool.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#include "rive_file_reader.hpp"
#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

using namespace rive;

// Hosts (the Flutter viewer, the web runtime, the command server, Unity) drive
// a scene through its advanceAndApply alone, which for a state machine reaches
// the artboard through advanceInternal rather than Artboard::advance. It must
// poll the pool itself, or a script's decodeImage/fetch promise never settles.

namespace
{
class HostTask : public WorkTask
{
public:
    bool completed = false;

    bool execute() override { return true; }
    void onComplete() override { completed = true; }
};

// Delivers whatever an earlier test left in the global pool, so these see
// only their own tasks.
void drainGlobalAsyncWork()
{
    for (int i = 0; i < 100 && rive_hasPendingAsyncWork(); i++)
    {
        rive_pollAsyncWork();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE_FALSE(rive_hasPendingAsyncWork());
}

// ball_test's "Artboard 2" settles once its nested one-shot animation ends
// (see nested_artboard_test).
std::unique_ptr<StateMachineInstance> settledMachine(ArtboardInstance* artboard)
{
    artboard->advance(0.0f);
    auto stateMachine = artboard->stateMachineAt(0);
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.9f);
    stateMachine->advanceAndApply(0.1f);
    stateMachine->advanceAndApply(0.1f);
    REQUIRE(stateMachine->advanceAndApply(0.1f) == false);
    return stateMachine;
}
} // namespace

TEST_CASE("state machine advanceAndApply delivers async work", "[workpool]")
{
    drainGlobalAsyncWork();
    auto file = ReadRiveFile("assets/ball_test.riv");
    auto artboard = file->artboardNamed("Artboard 2");
    auto stateMachine = settledMachine(artboard.get());

    auto task = make_rcp<HostTask>();
    getGlobalWorkPool()->submit(task);
    for (int frame = 0; frame < 1000 && !task->completed; frame++)
    {
        stateMachine->advanceAndApply(0.1f);
        if (!task->completed)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    CHECK(task->completed);
}

#ifdef WITH_RIVE_SCRIPTING_LUAU
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/lua/scripting_vm.hpp"

namespace
{
// More than one poll delivers, so part of a batch is still pending after the
// frame that polls it, whether the pool runs tasks on workers or in the poll.
constexpr int kOverBudgetTasks = 100;

// Gives the file a VM before instancing, as importing a file with scripts
// does, so the artboard's async work is scoped to that VM's owner id.
uint64_t addScriptingVM(File* file)
{
    file->setScriptingVM(make_rcp<ScriptingVM>(
        std::make_unique<CPPRuntimeScriptingContext>(&gNoOpFactory)));
    return file->scriptingVM()->context()->acquireOwnerId();
}

std::vector<rcp<HostTask>> submitTasks(uint64_t ownerId, int count)
{
    std::vector<rcp<HostTask>> tasks;
    for (int i = 0; i < count; i++)
    {
        auto task = make_rcp<HostTask>();
        task->setOwnerId(ownerId);
        tasks.push_back(task);
        getGlobalWorkPool()->submit(task);
    }
    return tasks;
}

// Ticks scene until tasks all land, counting frames that reported settled
// while some were still out.
int settledWhilePending(Scene* scene, const std::vector<rcp<HostTask>>& tasks)
{
    auto allDelivered = [&tasks] {
        return std::all_of(tasks.begin(),
                           tasks.end(),
                           [](const rcp<HostTask>& t) { return t->completed; });
    };
    int frames = 0;
    int settled = 0;
    for (; frames < 1000 && !allDelivered(); frames++)
    {
        bool keepGoing = scene->advanceAndApply(0.1f);
        if (!allDelivered())
        {
            settled += keepGoing ? 0 : 1;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    CHECK(allDelivered());
    // The batch outlasted a frame, so a frame did report on pending work.
    CHECK(frames > 1);
    return settled;
}
} // namespace

// Reporting "keep going" while the work is out matters to hosts that stop
// ticking a settled scene: without it they'd never poll the result in.
TEST_CASE("a settled machine keeps going until its file's async work lands",
          "[workpool]")
{
    drainGlobalAsyncWork();
    auto file = ReadRiveFile("assets/ball_test.riv");
    uint64_t owner = addScriptingVM(file.get());
    auto artboard = file->artboardNamed("Artboard 2");
    auto stateMachine = settledMachine(artboard.get());

    auto tasks = submitTasks(owner, kOverBudgetTasks);
    CHECK(settledWhilePending(stateMachine.get(), tasks) == 0);
    CHECK(stateMachine->advanceAndApply(0.1f) == false);
}

TEST_CASE("a finished animation keeps going until its file's async work lands",
          "[workpool]")
{
    drainGlobalAsyncWork();
    auto file = ReadRiveFile("assets/ball_test.riv");
    uint64_t owner = addScriptingVM(file.get());
    // A one-shot, unlike the looping Bounce in "Nested Artboard".
    auto artboard = file->artboardNamed("Nested Artboard 2");
    auto animation = artboard->animationNamed("Bounce");
    animation->advanceAndApply(0.0f);
    REQUIRE(animation->advanceAndApply(100.0f) == false);

    auto tasks = submitTasks(owner, kOverBudgetTasks);
    CHECK(settledWhilePending(animation.get(), tasks) == 0);
    CHECK(animation->advanceAndApply(0.1f) == false);
}

// An artboard bound in from another file runs its scripts on that file's VM,
// so the host's own VM doesn't see their work.
TEST_CASE("a settled machine keeps going until a bound file's async work lands",
          "[workpool]")
{
    drainGlobalAsyncWork();
    // Loading the asset twice yields two independent files, each with a VM.
    auto file = ReadRiveFile("assets/swappable_artboards_focus.riv");
    auto otherFile = ReadRiveFile("assets/swappable_artboards_focus.riv");
    addScriptingVM(file.get());
    uint64_t otherOwner = addScriptingVM(otherFile.get());

    auto artboard = file->artboardNamed("Main");
    auto stateMachine = artboard->stateMachineAt(0);
    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    stateMachine->bindViewModelInstance(vmi);
    vmi->propertyValue("artboardProp")
        ->as<ViewModelInstanceArtboard>()
        ->asset(otherFile->bindableArtboardNamed("Swappable1"));
    bool settled = false;
    for (int frame = 0; frame < 100 && !settled; frame++)
    {
        settled = !stateMachine->advanceAndApply(0.1f);
    }
    REQUIRE(settled);

    auto tasks = submitTasks(otherOwner, kOverBudgetTasks);
    CHECK(settledWhilePending(stateMachine.get(), tasks) == 0);
    CHECK(stateMachine->advanceAndApply(0.1f) == false);
}

// The command server and the Android command queue read false as settled, so
// another file's slow decode or hung fetch must not hold this machine up.
TEST_CASE("another owner's async work doesn't hold a settled machine",
          "[workpool]")
{
    drainGlobalAsyncWork();
    auto file = ReadRiveFile("assets/ball_test.riv");
    addScriptingVM(file.get());
    auto artboard = file->artboardNamed("Artboard 2");
    auto stateMachine = settledMachine(artboard.get());

    auto tasks = submitTasks(WorkPool::nextOwnerId(), kOverBudgetTasks);
    bool keepGoing = stateMachine->advanceAndApply(0.1f);
    // One frame's poll can't deliver the whole batch...
    CHECK(rive_hasPendingAsyncWork());
    // ...but none of it is this file's.
    CHECK_FALSE(keepGoing);

    drainGlobalAsyncWork();
}
#endif
