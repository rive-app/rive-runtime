/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include <atomic>
#include <string>

namespace rive
{

/// Status of a WorkTask.
enum class WorkStatus : uint8_t
{
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled
};

/// Base class for a unit of async work.
///
/// With WITH_RIVE_THREADING, execute() runs on a worker thread and
/// onComplete()/onError() are called on the main thread during
/// pollCompletedWork().
///
/// Without threading, execute() runs synchronously on the main thread
/// inside pollCompletedWork(), followed immediately by onComplete()/onError().
class WorkTask : public RefCnt<WorkTask>
{
public:
    virtual ~WorkTask() = default;

    /// Perform the work.  Called on worker thread (or main thread if no
    /// threading).  Return true on success, false on failure (set
    /// m_errorMessage before returning false).
    virtual bool execute() = 0;

    /// Called on the main thread after execute() returns true.
    virtual void onComplete() {}

    /// Called on the main thread after execute() returns false.
    virtual void onError(const std::string& error) {}

    /// Called when the task is cancelled (e.g. Lua state closing).
    virtual void onCancel() {}

    WorkStatus status() const { return m_status; }
    void setStatus(WorkStatus s) { m_status = s; }

    bool isCancelled() const
    {
        return m_cancelled.load(std::memory_order_acquire);
    }
    void cancel() { m_cancelled.store(true, std::memory_order_release); }

    const std::string& errorMessage() const { return m_errorMessage; }

    uint64_t ownerId() const { return m_ownerId; }
    void setOwnerId(uint64_t id) { m_ownerId = id; }

    uint64_t submitGeneration() const { return m_submitGeneration; }
    void setSubmitGeneration(uint64_t gen) { m_submitGeneration = gen; }

protected:
    std::string m_errorMessage;

private:
    WorkStatus m_status = WorkStatus::Pending;
    std::atomic<bool> m_cancelled{false};
    uint64_t m_ownerId = 0;
    uint64_t m_submitGeneration = 0;
};

} // namespace rive
