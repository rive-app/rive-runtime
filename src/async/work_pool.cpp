/*
 * Copyright 2026 Rive
 */

#include "rive/async/work_pool.hpp"
#include <algorithm>
#include <mutex>

namespace rive
{

std::atomic<uint64_t> WorkPool::s_nextOwnerId{1};

uint64_t WorkPool::nextOwnerId()
{
    return s_nextOwnerId.fetch_add(1, std::memory_order_relaxed);
}

// ============================================================================
// No-threading implementation
// ============================================================================

#ifndef WITH_RIVE_THREADING

WorkPool::WorkPool() {}

WorkPool::~WorkPool()
{
    // Deliver onCancel for tasks whose cancel flag was already set
    // (e.g. by cancelAllForOwner) but never polled. Tasks that were
    // never cancelled are silently dropped — calling virtual callbacks
    // during destruction is unsafe if dependent state (e.g. Lua VM)
    // has already been torn down.
    for (auto& task : m_workQueue)
    {
        if (task->isCancelled())
        {
            task->setStatus(WorkStatus::Cancelled);
            task->onCancel();
        }
    }
    m_workQueue.clear();
}

uint64_t WorkPool::submit(rcp<WorkTask> task)
{
    if (!task)
        return 0;
    task->setStatus(WorkStatus::Pending);
    m_workQueue.push_back(std::move(task));
    return m_nextHandle++;
}

uint32_t WorkPool::pollCompletedWork(uint32_t maxCallbacks)
{
    uint32_t processed = 0;

    while (processed < maxCallbacks && !m_workQueue.empty())
    {
        auto task = std::move(m_workQueue.front());
        m_workQueue.pop_front();

        if (task->isCancelled())
        {
            task->setStatus(WorkStatus::Cancelled);
            task->onCancel();
            processed++;
            continue;
        }

        task->setStatus(WorkStatus::Running);
        bool success = task->execute();

        if (task->isCancelled())
        {
            task->setStatus(WorkStatus::Cancelled);
            task->onCancel();
        }
        else if (success)
        {
            task->setStatus(WorkStatus::Completed);
            task->onComplete();
        }
        else
        {
            task->setStatus(WorkStatus::Failed);
            task->onError(task->errorMessage());
        }
        processed++;
    }
    return processed;
}

bool WorkPool::hasPendingWork() const { return !m_workQueue.empty(); }

void WorkPool::cancelAllForOwner(uint64_t ownerId)
{
    // Only set the cancel flag here. onCancel() is delivered once from
    // pollCompletedWork() when the task is dequeued, avoiding double-cancel.
    for (auto& task : m_workQueue)
    {
        if (task->ownerId() == ownerId)
            task->cancel();
    }
}

// ============================================================================
// Threaded implementation
// ============================================================================

#else // WITH_RIVE_THREADING

WorkPool::WorkPool()
{
    unsigned int n = std::max(std::thread::hardware_concurrency(), 1u);
    // Cap at a reasonable number for image decode.
    n = std::min(n, 4u);
    for (unsigned int i = 0; i < n; i++)
    {
        m_threads.emplace_back(&WorkPool::workerLoop, this);
    }
}

WorkPool::~WorkPool()
{
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_shutdown = true;
    }
    m_haveWork.notify_all();
    for (auto& t : m_threads)
    {
        if (t.joinable())
            t.join();
    }
    // Deliver onCancel for tasks whose cancel flag was already set but
    // never polled. Tasks that were never cancelled are silently dropped —
    // calling virtual callbacks during destruction is unsafe if dependent
    // state (e.g. Lua VM) has already been torn down.
    for (auto& task : m_completedQueue)
    {
        if (task->isCancelled())
        {
            task->setStatus(WorkStatus::Cancelled);
            task->onCancel();
        }
    }
    m_completedQueue.clear();
    for (auto& task : m_workQueue)
    {
        if (task->isCancelled())
        {
            task->setStatus(WorkStatus::Cancelled);
            task->onCancel();
        }
    }
    m_workQueue.clear();
}

uint64_t WorkPool::submit(rcp<WorkTask> task)
{
    if (!task)
        return 0;
    uint64_t handle;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        task->setStatus(WorkStatus::Pending);
        task->setSubmitGeneration(m_cancelGeneration);
        handle = m_nextHandle++;
        m_workQueue.push_back(std::move(task));
    }
    m_haveWork.notify_one();
    return handle;
}

void WorkPool::workerLoop()
{
    while (true)
    {
        rcp<WorkTask> task;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_haveWork.wait(lock, [this] {
                return m_shutdown || !m_workQueue.empty();
            });
            if (m_shutdown && m_workQueue.empty())
                return;
            task = std::move(m_workQueue.front());
            m_workQueue.pop_front();
        }

        m_inFlightCount.fetch_add(1, std::memory_order_relaxed);

        if (task->isCancelled())
        {
            task->setStatus(WorkStatus::Cancelled);
            std::lock_guard<std::mutex> lock(m_completedMutex);
            m_completedQueue.push_back(std::move(task));
            m_inFlightCount.fetch_sub(1, std::memory_order_relaxed);
            continue;
        }

        task->setStatus(WorkStatus::Running);
        bool success = task->execute();

        if (task->isCancelled())
            task->setStatus(WorkStatus::Cancelled);
        else if (success)
            task->setStatus(WorkStatus::Completed);
        else
            task->setStatus(WorkStatus::Failed);

        {
            std::lock_guard<std::mutex> lock(m_completedMutex);
            m_completedQueue.push_back(std::move(task));
        }
        m_inFlightCount.fetch_sub(1, std::memory_order_relaxed);
    }
}

uint32_t WorkPool::pollCompletedWork(uint32_t maxCallbacks)
{
    uint32_t processed = 0;

    while (processed < maxCallbacks)
    {
        rcp<WorkTask> task;
        {
            std::lock_guard<std::mutex> lock(m_completedMutex);
            if (m_completedQueue.empty())
                break;
            task = std::move(m_completedQueue.front());
            m_completedQueue.pop_front();
        }

        bool ownerCancelled = false;
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            auto it = m_cancelledOwners.find(task->ownerId());
            if (it != m_cancelledOwners.end())
            {
                // Task is owner-cancelled only if it was submitted
                // before the cancel call (submit gen < cancel gen).
                ownerCancelled = task->submitGeneration() < it->second;
            }
        }

        if (!task->isCancelled() && !ownerCancelled)
        {
            if (task->status() == WorkStatus::Completed)
                task->onComplete();
            else if (task->status() == WorkStatus::Failed)
                task->onError(task->errorMessage());
        }
        else
        {
            task->setStatus(WorkStatus::Cancelled);
            task->onCancel();
        }
        processed++;
    }
    return processed;
}

bool WorkPool::hasPendingWork() const
{
    if (m_inFlightCount.load(std::memory_order_relaxed) > 0)
        return true;
    {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_queueMutex));
        if (!m_workQueue.empty())
            return true;
    }
    {
        std::lock_guard<std::mutex> lock(
            const_cast<std::mutex&>(m_completedMutex));
        if (!m_completedQueue.empty())
            return true;
    }
    return false;
}

void WorkPool::cancelAllForOwner(uint64_t ownerId)
{
    // Only set the cancel flag under the lock. Do NOT call the virtual
    // onCancel() here — that risks deadlocks (virtual code under mutex)
    // and double-cancel (pollCompletedWork also delivers onCancel).
    // The single onCancel() delivery happens from pollCompletedWork()
    // on the main thread when the task is dequeued.
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        // Bump the cancel generation so that tasks submitted after this
        // call are not treated as cancelled. In-flight worker tasks
        // submitted before this point have submitGeneration < cancelGen
        // and will be caught by pollCompletedWork.
        ++m_cancelGeneration;
        m_cancelledOwners[ownerId] = m_cancelGeneration;
        for (auto& task : m_workQueue)
        {
            if (task->ownerId() == ownerId)
                task->cancel();
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_completedMutex);
        for (auto& task : m_completedQueue)
        {
            if (task->ownerId() == ownerId)
                task->cancel();
        }
    }
}

#endif // WITH_RIVE_THREADING

// ============================================================================
// Global singleton + polling (shared by both threading modes)
// ============================================================================

static rcp<WorkPool>& globalWorkPoolStorage()
{
    static rcp<WorkPool> s_workPool;
    return s_workPool;
}

rcp<WorkPool>& getGlobalWorkPool()
{
    // Thread-safe lazy initialization via std::call_once.
    static std::once_flag s_initFlag;
    auto& pool = globalWorkPoolStorage();
    std::call_once(s_initFlag, [&pool] { pool = make_rcp<WorkPool>(); });
    return pool;
}

rcp<WorkPool>& getGlobalWorkPoolIfExists() { return globalWorkPoolStorage(); }

void rive_pollAsyncWork()
{
    auto& pool = getGlobalWorkPoolIfExists();
    if (pool && pool->hasPendingWork())
    {
        pool->pollCompletedWork(16);
    }
}

} // namespace rive
