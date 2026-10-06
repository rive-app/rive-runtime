
#include "catch.hpp"
#include "rive/async/work_pool.hpp"

using namespace rive;

// A simple task that records its lifecycle.
class TestTask : public WorkTask
{
public:
    bool shouldSucceed = true;
    bool executed = false;
    bool completed = false;
    bool errored = false;
    bool cancelled = false;
    std::string errorMsg;

    bool execute() override
    {
        executed = true;
        if (!shouldSucceed)
        {
            m_errorMessage = "test failure";
            return false;
        }
        return true;
    }

    void onComplete() override { completed = true; }

    void onError(const std::string& error) override
    {
        errored = true;
        errorMsg = error;
    }

    void onCancel() override { cancelled = true; }
};

// ============================================================================
// Basic lifecycle
// ============================================================================

TEST_CASE("WorkPool executes task on poll", "[workpool]")
{
    WorkPool pool;
    auto task = make_rcp<TestTask>();
    pool.submit(task);

    CHECK(task->executed == false);
    CHECK(pool.hasPendingWork());

    pool.pollCompletedWork(1);

    CHECK(task->executed == true);
    CHECK(task->completed == true);
    CHECK(task->errored == false);
    CHECK(task->cancelled == false);
    CHECK_FALSE(pool.hasPendingWork());
}

TEST_CASE("WorkPool delivers onError for failed tasks", "[workpool]")
{
    WorkPool pool;
    auto task = make_rcp<TestTask>();
    task->shouldSucceed = false;
    pool.submit(task);

    pool.pollCompletedWork(1);

    CHECK(task->executed == true);
    CHECK(task->completed == false);
    CHECK(task->errored == true);
    CHECK(task->errorMsg == "test failure");
}

TEST_CASE("WorkPool respects maxCallbacks", "[workpool]")
{
    WorkPool pool;
    auto t1 = make_rcp<TestTask>();
    auto t2 = make_rcp<TestTask>();
    auto t3 = make_rcp<TestTask>();
    pool.submit(t1);
    pool.submit(t2);
    pool.submit(t3);

    uint32_t processed = pool.pollCompletedWork(2);

    CHECK(processed == 2);
    CHECK(t1->completed == true);
    CHECK(t2->completed == true);
    CHECK(t3->completed == false);
    CHECK(pool.hasPendingWork());

    pool.pollCompletedWork(1);
    CHECK(t3->completed == true);
    CHECK_FALSE(pool.hasPendingWork());
}

// ============================================================================
// Cancellation
// ============================================================================

TEST_CASE("cancelAllForOwner marks tasks cancelled", "[workpool]")
{
    WorkPool pool;
    auto t1 = make_rcp<TestTask>();
    auto t2 = make_rcp<TestTask>();
    t1->setOwnerId(42);
    t2->setOwnerId(99);
    pool.submit(t1);
    pool.submit(t2);

    pool.cancelAllForOwner(42);

    // t1 should be cancelled, t2 should still run.
    pool.pollCompletedWork(16);

    CHECK(t1->cancelled == true);
    CHECK(t1->completed == false);
    CHECK(t1->executed == false); // cancelled before execute

    CHECK(t2->cancelled == false);
    CHECK(t2->completed == true);
    CHECK(t2->executed == true);
}

TEST_CASE("onCancel is delivered exactly once", "[workpool]")
{
    // Regression test: cancelAllForOwner should NOT call onCancel(),
    // only set the flag. pollCompletedWork delivers the single onCancel.
    int cancelCount = 0;

    class CountingTask : public WorkTask
    {
    public:
        int* counter;
        CountingTask(int* c) : counter(c) {}
        bool execute() override { return true; }
        void onCancel() override { (*counter)++; }
    };

    WorkPool pool;
    auto task = make_rcp<CountingTask>(&cancelCount);
    task->setOwnerId(1);
    pool.submit(task);

    pool.cancelAllForOwner(1);
    pool.pollCompletedWork(16);

    CHECK(cancelCount == 1);
}

TEST_CASE("cancelled owner does not interfere with other owners", "[workpool]")
{
    WorkPool pool;

    auto ownerA1 = make_rcp<TestTask>();
    auto ownerA2 = make_rcp<TestTask>();
    auto ownerB1 = make_rcp<TestTask>();
    ownerA1->setOwnerId(10);
    ownerA2->setOwnerId(10);
    ownerB1->setOwnerId(20);
    pool.submit(ownerA1);
    pool.submit(ownerB1);
    pool.submit(ownerA2);

    pool.cancelAllForOwner(10);
    pool.pollCompletedWork(16);

    // Owner 10's tasks cancelled.
    CHECK(ownerA1->cancelled == true);
    CHECK(ownerA1->completed == false);
    CHECK(ownerA2->cancelled == true);
    CHECK(ownerA2->completed == false);

    // Owner 20's task runs normally.
    CHECK(ownerB1->cancelled == false);
    CHECK(ownerB1->completed == true);
    CHECK(ownerB1->executed == true);
}

TEST_CASE("cancel only affects tasks present at time of call", "[workpool]")
{
    // Tasks submitted after cancelAllForOwner should NOT be cancelled —
    // cancellation is point-in-time, not sticky.
    WorkPool pool;

    auto t1 = make_rcp<TestTask>();
    t1->setOwnerId(5);
    pool.submit(t1);
    pool.cancelAllForOwner(5);

    // Submit another task for the same owner after cancellation.
    auto t2 = make_rcp<TestTask>();
    t2->setOwnerId(5);
    pool.submit(t2);

    pool.pollCompletedWork(16);

    CHECK(t1->cancelled == true);
    // t2 wasn't in the queue when cancelAllForOwner ran, so its flag
    // isn't set. It will execute normally. This is expected — cancel
    // only affects tasks present at the time of the call.
    CHECK(t2->executed == true);
    CHECK(t2->completed == true);
}

// ============================================================================
// Owner IDs
// ============================================================================

TEST_CASE("nextOwnerId generates unique IDs", "[workpool]")
{
    uint64_t a = WorkPool::nextOwnerId();
    uint64_t b = WorkPool::nextOwnerId();
    CHECK(a != b);
    CHECK(b == a + 1);
}

// ============================================================================
// In-flight tracking
// ============================================================================

#ifdef WITH_RIVE_THREADING
#include <mutex>
#include <condition_variable>

// A task that blocks in execute() until signalled, so we can observe
// in-flight state.
class BlockingTask : public WorkTask
{
public:
    std::mutex mtx;
    std::condition_variable cv;
    bool unblock = false;
    bool completed = false;
    bool executed = false;

    bool execute() override
    {
        executed = true;
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return unblock; });
        return true;
    }
    void onComplete() override { completed = true; }
};

TEST_CASE("hasPendingWork is true while task is in-flight", "[workpool]")
{
    WorkPool pool;
    auto task = make_rcp<BlockingTask>();
    pool.submit(task);

    // Spin until the worker picks up the task.
    while (!task->executed)
        std::this_thread::yield();

    // Task is executing on worker — neither in workQueue nor completedQueue.
    // hasPendingWork() must still return true.
    CHECK(pool.hasPendingWork());

    // Unblock the task.
    {
        std::lock_guard<std::mutex> lock(task->mtx);
        task->unblock = true;
    }
    task->cv.notify_one();

    // Poll to drain.
    while (pool.hasPendingWork())
        pool.pollCompletedWork(16);

    CHECK(task->completed);
}

TEST_CASE("owner-cancel of in-flight task sets status to Cancelled",
          "[workpool]")
{
    WorkPool pool;
    auto task = make_rcp<BlockingTask>();
    task->setOwnerId(77);
    pool.submit(task);

    // Wait until the worker starts executing.
    while (!task->executed)
        std::this_thread::yield();

    // Cancel the owner while the task is in-flight.
    pool.cancelAllForOwner(77);

    // Unblock the task so the worker can finish.
    {
        std::lock_guard<std::mutex> lock(task->mtx);
        task->unblock = true;
    }
    task->cv.notify_one();

    // Poll — should deliver onCancel, and status should be Cancelled.
    while (pool.hasPendingWork())
        pool.pollCompletedWork(16);

    CHECK(task->status() == WorkStatus::Cancelled);
    // onComplete should NOT have been called.
    CHECK(task->completed == false);
}
#endif // WITH_RIVE_THREADING

// ============================================================================
// Destructor cleanup
// ============================================================================

TEST_CASE("Destructor delivers onCancel for pre-cancelled tasks", "[workpool]")
{
    // A task that was explicitly cancelled (e.g. via cancelAllForOwner)
    // but never polled should get onCancel() in the destructor.
    auto t1 = make_rcp<TestTask>();
    t1->setOwnerId(42);
    auto t2 = make_rcp<TestTask>();
    {
        WorkPool pool;
        pool.submit(t1);
        pool.submit(t2);
        pool.cancelAllForOwner(42);
        // Destroy pool without polling.
    }
    CHECK(t1->cancelled == true);
    CHECK(t1->completed == false);
    // t2 was never cancelled — destructor silently drops it (no virtual
    // callback during destruction, since dependent state may be gone).
    CHECK(t2->cancelled == false);
    CHECK(t2->completed == false);
}

// ============================================================================
// Cancelled owner cleanup
// ============================================================================

TEST_CASE("cancelled owner does not block future tasks with same owner",
          "[workpool]")
{
    // After cancelling owner 5 and polling all tasks, submitting new
    // tasks for owner 5 should work normally — the cancel is not sticky.
    WorkPool pool;

    auto t1 = make_rcp<TestTask>();
    t1->setOwnerId(5);
    pool.submit(t1);
    pool.cancelAllForOwner(5);
    pool.pollCompletedWork(16);
    CHECK(t1->cancelled == true);

    // Now submit a new task for owner 5.
    auto t2 = make_rcp<TestTask>();
    t2->setOwnerId(5);
    pool.submit(t2);
    pool.pollCompletedWork(16);

    // t2 must NOT be treated as cancelled.
    CHECK(t2->completed == true);
    CHECK(t2->cancelled == false);
}

// ============================================================================
// Empty pool
// ============================================================================

TEST_CASE("Empty pool has no pending work", "[workpool]")
{
    WorkPool pool;
    CHECK_FALSE(pool.hasPendingWork());
    CHECK(pool.pollCompletedWork(16) == 0);
}

// ============================================================================
// Per-owner pending work
// ============================================================================

TEST_CASE("hasPendingWorkForOwner sees only that owner's tasks", "[workpool]")
{
    WorkPool pool;
    uint64_t owner = WorkPool::nextOwnerId();
    uint64_t other = WorkPool::nextOwnerId();
    auto task = make_rcp<TestTask>();
    task->setOwnerId(owner);
    pool.submit(task);

    CHECK(pool.hasPendingWorkForOwner(owner));
    CHECK_FALSE(pool.hasPendingWorkForOwner(other));

    while (pool.hasPendingWork())
        pool.pollCompletedWork(16);

    CHECK(task->completed);
    CHECK_FALSE(pool.hasPendingWorkForOwner(owner));
}

TEST_CASE("hasPendingWorkForOwner holds a cancelled task until it's delivered",
          "[workpool]")
{
    WorkPool pool;
    uint64_t owner = WorkPool::nextOwnerId();
    auto task = make_rcp<TestTask>();
    task->setOwnerId(owner);
    pool.submit(task);
    pool.cancelAllForOwner(owner);

    CHECK(pool.hasPendingWorkForOwner(owner));

    while (pool.hasPendingWork())
        pool.pollCompletedWork(16);

    CHECK(task->cancelled);
    CHECK_FALSE(pool.hasPendingWorkForOwner(owner));
}

// ============================================================================
// Host frame loop
// ============================================================================

#include "rive/animation/state_machine_instance.hpp"
#include "rive_file_reader.hpp"
#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

// Hosts (the Flutter viewer, the web runtime, the command server, Unity) drive
// a scene through StateMachineInstance::advanceAndApply alone, which reaches
// the artboard through advanceInternal rather than Artboard::advance. It must
// poll the pool itself, or a script's decodeImage/fetch promise never settles.

namespace
{
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

    auto task = make_rcp<TestTask>();
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

std::vector<rcp<TestTask>> submitTasks(uint64_t ownerId, int count)
{
    std::vector<rcp<TestTask>> tasks;
    for (int i = 0; i < count; i++)
    {
        auto task = make_rcp<TestTask>();
        task->setOwnerId(ownerId);
        tasks.push_back(task);
        getGlobalWorkPool()->submit(task);
    }
    return tasks;
}
} // namespace

// Reporting "keep going" while the work is out matters to hosts that stop
// ticking a settled machine: without it they'd never poll the result in.
TEST_CASE("a settled machine keeps going until its file's async work lands",
          "[workpool]")
{
    drainGlobalAsyncWork();
    auto file = ReadRiveFile("assets/ball_test.riv");
    uint64_t owner = addScriptingVM(file.get());
    auto artboard = file->artboardNamed("Artboard 2");
    auto stateMachine = settledMachine(artboard.get());

    auto tasks = submitTasks(owner, kOverBudgetTasks);
    auto allDelivered = [&tasks] {
        return std::all_of(tasks.begin(),
                           tasks.end(),
                           [](const rcp<TestTask>& t) { return t->completed; });
    };
    int frames = 0;
    int settledWhilePending = 0;
    for (; frames < 1000 && !allDelivered(); frames++)
    {
        bool keepGoing = stateMachine->advanceAndApply(0.1f);
        if (!allDelivered())
        {
            settledWhilePending += keepGoing ? 0 : 1;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    CHECK(allDelivered());
    // The batch outlasted a frame, so a frame did report on pending work.
    CHECK(frames > 1);
    CHECK(settledWhilePending == 0);
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
