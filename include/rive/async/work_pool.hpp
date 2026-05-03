/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/async/work_task.hpp"
#include "rive/refcnt.hpp"
#include <atomic>
#include <deque>
#include <cstdint>

#ifdef WITH_RIVE_THREADING
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <unordered_map>
#endif

namespace rive
{

/// A pool that executes WorkTasks.
///
/// WITH_RIVE_THREADING: spawns worker threads; submit() queues work;
/// pollCompletedWork() delivers callbacks on the main thread.
///
/// Without threading: submit() just enqueues; pollCompletedWork() dequeues,
/// calls execute() synchronously, then delivers callbacks — all on the main
/// thread.
class WorkPool : public RefCnt<WorkPool>
{
public:
    WorkPool();
    ~WorkPool();

    /// Submit a task for execution.  Returns a nonzero handle on success.
    uint64_t submit(rcp<WorkTask> task);

    /// Process up to maxCallbacks completed tasks on the main thread.
    /// Returns the number of tasks processed.
    uint32_t pollCompletedWork(uint32_t maxCallbacks = 16);

    /// Returns true if any tasks are queued, running, or
    /// completed-but-unpolled.
    bool hasPendingWork() const;

    /// Cancel all tasks belonging to the given owner.
    void cancelAllForOwner(uint64_t ownerId);

    /// Generate a unique owner ID for task tracking.
    static uint64_t nextOwnerId();

private:
    std::deque<rcp<WorkTask>> m_workQueue;

#ifdef WITH_RIVE_THREADING
    std::deque<rcp<WorkTask>> m_completedQueue;
    std::vector<std::thread> m_threads;
    std::mutex m_queueMutex;
    std::mutex m_completedMutex;
    std::condition_variable m_haveWork;
    // Maps ownerId → cancel generation. A task is owner-cancelled only if
    // its submit generation is less than the owner's cancel generation.
    std::unordered_map<uint64_t, uint64_t> m_cancelledOwners;
    std::atomic<int> m_inFlightCount{0};
    uint64_t m_cancelGeneration = 0;
    bool m_shutdown = false;

    void workerLoop();
#endif

    uint64_t m_nextHandle = 1;
    static std::atomic<uint64_t> s_nextOwnerId;
};

/// Returns the process-global WorkPool singleton. Creates it on first call.
rcp<WorkPool>& getGlobalWorkPool();

/// Returns the global WorkPool if it has been created, or an empty rcp if not.
rcp<WorkPool>& getGlobalWorkPoolIfExists();

/// Poll the global WorkPool for completed async tasks (image decodes, etc.).
/// Called from Artboard::advance() so promises resolve before script callbacks.
/// Safe to call even if no WorkPool exists (no-op).
void rive_pollAsyncWork();

} // namespace rive
