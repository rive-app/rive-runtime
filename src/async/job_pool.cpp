/*
 * Copyright 2026 Rive
 */

#include "rive/async/job_pool.hpp"
#include "rive/async/timeouts.hpp"

#include <chrono>

using namespace rive;

using Clock = std::chrono::steady_clock;

static std::mutex& factoryMutex()
{
  static std::mutex mutex;
  return mutex;
}

static JobPool::ThreadFactory& threadFactory()
{
  static JobPool::ThreadFactory factory;
  return factory;
}

void JobPool::setThreadFactory(ThreadFactory factory)
{
  std::lock_guard<std::mutex> lock(factoryMutex());
  threadFactory() = std::move(factory);
}

std::thread JobPool::makeThread(std::function<void()> body)
{
  ThreadFactory factory;
  {
    std::lock_guard<std::mutex> lock(factoryMutex());
    factory = threadFactory();
  }
  return factory ? factory(std::move(body)) : std::thread(std::move(body));
}

JobPool::JobPool(uint32_t threadCount, JobSchedule schedule, uint64_t seed)
#ifdef WITH_RIVE_TOOLS
    : m_schedule(schedule)
    , m_rng(seed)
#endif
{
#ifndef WITH_RIVE_TOOLS
  (void)schedule;
  (void)seed;
#endif
#if RIVE_POOL_THREADS()
  m_threads.reserve(threadCount);
  for (uint32_t i = 0; i < threadCount; i++)
  {
    m_threads.push_back(makeThread([this]() { workerLoop(); }));
  }
#else
  (void)threadCount;
#endif
}

JobPool::~JobPool()
{
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stopping = true;
    for (auto& task : m_queue)
    {
      uint8_t expected = (uint8_t)JobTask::State::pending;
      compareExchangeStrong(
          task->m_state,
          expected,
          (uint8_t)JobTask::State::cancelled);
    }
    m_queue.clear();
  }
  m_workCv.notify_all();
  m_doneCv.notify_all();
  for (auto& thread : m_threads)
  {
    thread.join();
  }
}

void JobPool::submit(rcp<JobTask> task)
{
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.push_back(std::move(task));
  }
  m_workCv.notify_one();
}

rcp<JobTask> JobPool::takeLocked()
{
  while (!m_queue.empty())
  {
    rcp<JobTask> task;
#ifdef WITH_RIVE_TOOLS
    switch (m_schedule)
    {
      case JobSchedule::fifo:
        task = std::move(m_queue.front());
        m_queue.pop_front();
        break;
      case JobSchedule::reverse:
        task = std::move(m_queue.back());
        m_queue.pop_back();
        break;
      case JobSchedule::shuffle:
      {
        size_t index = (size_t)(m_rng() % m_queue.size());
        task = std::move(m_queue[index]);
        m_queue.erase(m_queue.begin() + index);
        break;
      }
    }
#else
    task = std::move(m_queue.front());
    m_queue.pop_front();
#endif
    // A joiner may have claimed it inline, or a cancel dropped it.
    if (task->state() == JobTask::State::pending)
    {
      return task;
    }
  }
  return nullptr;
}

void JobPool::workerLoop()
{
  for (;;)
  {
    rcp<JobTask> task;
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_workCv.wait(lock, [this]() { return m_stopping || !m_queue.empty(); });
      if (m_stopping)
      {
        return;
      }
      task = takeLocked();
    }
    if (task != nullptr && task->tryClaim())
    {
      execute(std::move(task));
    }
  }
}

void JobPool::execute(rcp<JobTask> task)
{
  task->run();
  // The claimer's reference goes before a joiner can see the task done.
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    task->m_state.store(
        (uint8_t)JobTask::State::done,
        std::memory_order_release);
    task = nullptr;
  }
  m_doneCv.notify_all();
}

void JobPool::notifyFinished()
{
  // Taking the lock orders the state store before a waiter's predicate.
  {
    std::lock_guard<std::mutex> lock(m_mutex);
  }
  m_doneCv.notify_all();
}

bool JobPool::join(JobTask* task, double timeoutSeconds, bool runHere)
{
  if (runHere && task->tryClaim())
  {
    execute(ref_rcp(task));
    return true;
  }
  if (threadCount() == 0)
  {
    return task->finished();
  }
  std::unique_lock<std::mutex> lock(m_mutex);
  auto done = [task]() { return task->finished(); };
  if (unboundedSeconds(timeoutSeconds))
  {
    m_doneCv.wait(lock, done);
    return true;
  }
  return m_doneCv.wait_until(lock, deadlineAfter(timeoutSeconds), done);
}

int JobPool::joinAny(Span<JobTask*> tasks, double timeoutSeconds, bool runHere)
{
  if (tasks.empty())
  {
    return -1;
  }
#if RIVE_POOL_THREADS()
  bool bounded = !unboundedSeconds(timeoutSeconds);
  auto deadline = deadlineAfter(bounded ? timeoutSeconds : 0);
#else
  (void)timeoutSeconds;
#endif
  for (;;)
  {
    for (size_t i = 0; i < tasks.size(); i++)
    {
      if (tasks[i]->finished())
      {
        return (int)i;
      }
    }
    for (size_t i = 0; runHere && i < tasks.size(); i++)
    {
      if (tasks[i]->tryClaim())
      {
        execute(ref_rcp(tasks[i]));
        return (int)i;
      }
    }
    if (threadCount() == 0)
    {
      return -1;
    }
#if RIVE_POOL_THREADS()
    std::unique_lock<std::mutex> lock(m_mutex);
    auto anyDone = [&tasks]() {
      for (JobTask* task : tasks)
      {
        if (task->finished())
        {
          return true;
        }
      }
      return false;
    };
    if (!bounded)
    {
      m_doneCv.wait(lock, anyDone);
    }
    else if (!m_doneCv.wait_until(lock, deadline, anyDone))
    {
      return -1;
    }
#endif
  }
}

void JobPool::cancel(JobTask* task)
{
  task->m_cancelRequested.store(true, std::memory_order_release);
  uint8_t expected = (uint8_t)JobTask::State::pending;
  if (compareExchangeStrong(
          task->m_state,
          expected,
          (uint8_t)JobTask::State::cancelled))
  {
    notifyFinished();
    return;
  }
  if (expected == (uint8_t)JobTask::State::running)
  {
    task->interrupt();
  }
}

uint32_t JobPool::runPending(uint32_t max)
{
  return runQueued(max, Clock::time_point::max());
}

uint32_t JobPool::runPendingFor(double seconds)
{
  return runQueued(UINT32_MAX, deadlineAfter(seconds));
}

uint32_t JobPool::runQueued(uint32_t max, Clock::time_point until)
{
  uint32_t ran = 0;
  while (ran < max && (ran == 0 || Clock::now() < until))
  {
    rcp<JobTask> task;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      task = takeLocked();
    }
    if (task == nullptr)
    {
      break;
    }
    if (task->tryClaim())
    {
      execute(std::move(task));
      ran++;
    }
  }
  return ran;
}

bool JobPool::hasQueued()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  for (auto& task : m_queue)
  {
    if (task->state() == JobTask::State::pending)
    {
      return true;
    }
  }
  return false;
}
