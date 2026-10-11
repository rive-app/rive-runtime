
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

#ifndef WITH_RIVE_THREADING
// Submits another task as it completes, as a decode listener starting the
// next decode does.
class ChainTask : public TestTask
{
public:
    WorkPool* pool = nullptr;
    rcp<WorkTask> next;

    void onComplete() override
    {
        TestTask::onComplete();
        pool->submit(next);
    }
};

TEST_CASE("Work submitted during delivery waits for the next poll",
          "[workpool]")
{
    WorkPool pool;
    auto second = make_rcp<TestTask>();
    auto first = make_rcp<ChainTask>();
    first->pool = &pool;
    first->next = second;
    pool.submit(first);

    CHECK(pool.pollCompletedWork(16) == 1);
    CHECK(first->completed);
    CHECK_FALSE(second->executed);
    CHECK(pool.hasPendingWork());

    CHECK(pool.pollCompletedWork(16) == 1);
    CHECK(second->completed);
    CHECK_FALSE(pool.hasPendingWork());
}

// Polls again as it completes, as a promise callback advancing a scripted
// artboard does.
class NestedPollTask : public TestTask
{
public:
    WorkPool* pool = nullptr;
    uint32_t nested = 0;

    void onComplete() override
    {
        TestTask::onComplete();
        nested = pool->pollCompletedWork(16);
    }
};

TEST_CASE("A poll nested in a completion leaves the outer poll consistent",
          "[workpool]")
{
    WorkPool pool;
    auto first = make_rcp<NestedPollTask>();
    first->pool = &pool;
    auto second = make_rcp<TestTask>();
    pool.submit(first);
    pool.submit(second);

    // The nested poll delivers the second task; the outer one stops at the
    // emptied queue rather than reading past it.
    CHECK(pool.pollCompletedWork(16) == 1);
    CHECK(first->nested == 1);
    CHECK(first->completed);
    CHECK(second->completed);
    CHECK_FALSE(pool.hasPendingWork());
}

TEST_CASE("Work submitted during a nested poll waits for a later poll",
          "[workpool]")
{
    WorkPool pool;
    auto first = make_rcp<NestedPollTask>();
    first->pool = &pool;
    auto second = make_rcp<ChainTask>();
    auto third = make_rcp<TestTask>();
    second->pool = &pool;
    second->next = third;
    pool.submit(first);
    pool.submit(second);

    // The nested poll delivers the second task, which submits the third;
    // neither poll that was running then may run it.
    CHECK(pool.pollCompletedWork(16) == 1);
    CHECK(second->completed);
    CHECK_FALSE(third->executed);
    CHECK(pool.pollCompletedWork(16) == 1);
    CHECK(third->completed);
}
#endif

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
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

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
// Submits the next task as it is delivered and waits for a worker to finish
// it, so it completes while the poll delivering this one is still running.
class ChainWaitTask : public TestTask
{
public:
    WorkPool* pool = nullptr;
    rcp<TestTask> next;

    void onComplete() override
    {
        TestTask::onComplete();
        pool->submit(next);
        auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (next->status() != WorkStatus::Completed &&
               std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::yield();
        }
        // The worker queues it right after setting its status.
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
};

TEST_CASE("Work completing during delivery waits for the next poll",
          "[workpool]")
{
    WorkPool pool;
    auto second = make_rcp<TestTask>();
    auto first = make_rcp<ChainWaitTask>();
    first->pool = &pool;
    first->next = second;
    pool.submit(first);

    uint32_t delivered = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (delivered == 0 && std::chrono::steady_clock::now() < deadline)
    {
        delivered = pool.pollCompletedWork(16);
    }
    CHECK(delivered == 1);
    CHECK(first->completed);
    CHECK(second->executed);
    CHECK_FALSE(second->completed);

    CHECK(pool.pollCompletedWork(16) == 1);
    CHECK(second->completed);
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
