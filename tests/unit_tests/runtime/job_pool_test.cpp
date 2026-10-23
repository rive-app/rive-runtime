#include "catch.hpp"
#include "rive/async/job_pool.hpp"

#include <atomic>
#include <chrono>
#include <cmath>

using namespace rive;

namespace
{
class CountTask : public JobTask
{
public:
  std::atomic<int> runs{0};
  std::atomic<bool> interrupted{false};
  std::thread::id ranOn;

protected:
  void run() override
  {
    ranOn = std::this_thread::get_id();
    runs++;
  }
  void interrupt() override { interrupted = true; }
};

// Runs until released, so a test can hold a pool thread.
class GateTask : public JobTask
{
public:
  std::atomic<bool> release{false};
  std::atomic<bool> started{false};
  std::atomic<bool> interrupted{false};

protected:
  void run() override
  {
    started = true;
    while (!release.load() && !interrupted.load())
    {
      std::this_thread::yield();
    }
  }
  void interrupt() override { interrupted = true; }
};

const JobSchedule kSchedules[] = {
    JobSchedule::fifo,
    JobSchedule::reverse,
    JobSchedule::shuffle};

void waitStarted(GateTask* task)
{
  while (!task->started.load())
  {
    std::this_thread::yield();
  }
}
} // namespace

TEST_CASE("zero thread pool runs a joined task on the joiner", "[jobpool]")
{
  JobPool pool(0);
  auto task = make_rcp<CountTask>();
  pool.submit(task);
  CHECK(task->state() == JobTask::State::pending);
  CHECK(pool.join(task.get(), 0));
  CHECK(task->runs == 1);
  CHECK(task->ranOn == std::this_thread::get_id());
  CHECK(task->state() == JobTask::State::done);
}

TEST_CASE("zero thread pool runs queued tasks only when pumped", "[jobpool]")
{
  JobPool pool(0);
  auto a = make_rcp<CountTask>();
  auto b = make_rcp<CountTask>();
  pool.submit(a);
  pool.submit(b);
  CHECK(pool.hasQueued());
  CHECK(pool.runPending(1) == 1);
  CHECK(a->runs == 1);
  CHECK(b->runs == 0);
  CHECK(pool.runPending(8) == 1);
  CHECK(b->runs == 1);
  CHECK_FALSE(pool.hasQueued());
}

namespace
{
class SleepTask : public JobTask
{
public:
  explicit SleepTask(double seconds) : m_seconds(seconds) {}
  std::atomic<int> runs{0};

protected:
  void run() override
  {
    runs++;
    std::this_thread::sleep_for(std::chrono::duration<double>(m_seconds));
  }

private:
  double m_seconds;
};

} // namespace

TEST_CASE(
    "a timed pump runs one task, then more within its seconds",
    "[jobpool]")
{
  JobPool pool(0);
  auto slow = make_rcp<SleepTask>(0.05);
  auto quick = make_rcp<CountTask>();
  pool.submit(slow);
  pool.submit(quick);
  // The first runs to its end past the slice, which leaves no room.
  CHECK(pool.runPendingFor(0.01) == 1);
  CHECK(slow->runs == 1);
  CHECK(quick->runs == 0);
  auto another = make_rcp<CountTask>();
  pool.submit(another);
  CHECK(pool.runPendingFor(1) == 2);
  CHECK(quick->runs == 1);
  CHECK(another->runs == 1);
  CHECK_FALSE(pool.hasQueued());
}

TEST_CASE("a join that may not run a task here leaves it queued", "[jobpool]")
{
  JobPool pool(0);
  auto task = make_rcp<CountTask>();
  pool.submit(task);
  CHECK_FALSE(pool.join(task.get(), 0, false));
  JobTask* tasks[] = {task.get()};
  CHECK(pool.joinAny(tasks, 0, false) == -1);
  CHECK(task->runs == 0);
  CHECK(task->state() == JobTask::State::pending);
  CHECK(pool.runPending(8) == 1);
  CHECK(task->runs == 1);
  CHECK(task->state() == JobTask::State::done);
}

TEST_CASE("every task runs exactly once under every schedule", "[jobpool]")
{
  for (JobSchedule schedule : kSchedules)
  {
    for (uint32_t threads : {0u, 1u, 4u})
    {
      JobPool pool(threads, schedule, 7);
      std::vector<rcp<CountTask>> tasks;
      for (int i = 0; i < 200; i++)
      {
        tasks.push_back(make_rcp<CountTask>());
        pool.submit(tasks.back());
      }
      // Join in reverse so joiners race the pool threads for claims.
      for (size_t i = tasks.size(); i-- > 0;)
      {
        REQUIRE(pool.join(tasks[i].get(), INFINITY));
      }
      for (auto& task : tasks)
      {
        REQUIRE(task->runs == 1);
      }
    }
  }
}

TEST_CASE("join waits for a running task and honours its timeout", "[jobpool]")
{
  JobPool pool(1);
  auto gate = make_rcp<GateTask>();
  pool.submit(gate);
  waitStarted(gate.get());
  auto start = std::chrono::steady_clock::now();
  CHECK_FALSE(pool.join(gate.get(), 0.02));
  CHECK(
      std::chrono::steady_clock::now() - start >=
      std::chrono::milliseconds(15));
  gate->release = true;
  CHECK(pool.join(gate.get(), INFINITY));
  CHECK(gate->state() == JobTask::State::done);
}

TEST_CASE(
    "joinAny returns a finished task or claims the lowest unstarted",
    "[jobpool]")
{
  JobPool pool(0);
  auto a = make_rcp<CountTask>();
  auto b = make_rcp<CountTask>();
  pool.submit(a);
  pool.submit(b);
  JobTask* tasks[] = {a.get(), b.get()};
  CHECK(pool.joinAny(tasks, 0) == 0);
  CHECK(a->runs == 1);
  CHECK(b->runs == 0);
  // a has finished, so it answers again without running b.
  CHECK(pool.joinAny(tasks, 0) == 0);
  CHECK(b->runs == 0);
  JobTask* onlyB[] = {b.get()};
  CHECK(pool.joinAny(onlyB, 0) == 0);
  CHECK(b->runs == 1);
}

TEST_CASE("joinAny times out while every task runs elsewhere", "[jobpool]")
{
  JobPool pool(2);
  auto a = make_rcp<GateTask>();
  auto b = make_rcp<GateTask>();
  pool.submit(a);
  pool.submit(b);
  waitStarted(a.get());
  waitStarted(b.get());
  JobTask* tasks[] = {a.get(), b.get()};
  CHECK(pool.joinAny(tasks, 0.01) == -1);
  b->release = true;
  CHECK(pool.joinAny(tasks, INFINITY) == 1);
  a->release = true;
  CHECK(pool.join(a.get(), INFINITY));
}

TEST_CASE(
    "a pool thread's reference goes before its joiner sees the task done",
    "[jobpool]")
{
  JobPool pool(4);
  std::atomic<int> unfinished{0};
  std::atomic<int> held{0};
  // Each joiner also wakes for the others' tasks, so one soon catches a
  // pool thread still holding a task it marked done.
  std::vector<std::thread> joiners;
  for (int i = 0; i < 4; i++)
  {
    joiners.emplace_back([&]() {
      for (int round = 0; round < 5000; round++)
      {
        auto task = make_rcp<CountTask>();
        pool.submit(task);
        unfinished += !pool.join(task.get(), 10.0, false);
        held += task->debugging_refcnt() != 1;
      }
    });
  }
  for (std::thread& joiner : joiners)
  {
    joiner.join();
  }
  CHECK(unfinished == 0);
  CHECK(held == 0);
}

TEST_CASE("cancelling a pending task drops it", "[jobpool]")
{
  JobPool pool(0);
  auto task = make_rcp<CountTask>();
  pool.submit(task);
  pool.cancel(task.get());
  CHECK(task->state() == JobTask::State::cancelled);
  CHECK(task->cancelRequested());
  CHECK(pool.join(task.get(), 0));
  CHECK(pool.runPending(8) == 0);
  CHECK(task->runs == 0);
}

TEST_CASE("cancelling a running task interrupts it", "[jobpool]")
{
  JobPool pool(1);
  auto gate = make_rcp<GateTask>();
  pool.submit(gate);
  waitStarted(gate.get());
  pool.cancel(gate.get());
  CHECK(pool.join(gate.get(), INFINITY));
  CHECK(gate->interrupted);
}

TEST_CASE("the thread factory makes the pool threads", "[jobpool]")
{
  std::atomic<int> made{0};
  JobPool::setThreadFactory([&made](std::function<void()> loop) {
    made++;
    return std::thread(std::move(loop));
  });
  {
    JobPool pool(3);
    CHECK(pool.threadCount() == 3);
    auto task = make_rcp<CountTask>();
    pool.submit(task);
    CHECK(pool.join(task.get(), INFINITY));
  }
  JobPool::setThreadFactory(nullptr);
  CHECK(made == 3);
}

TEST_CASE("destroying a pool cancels what never started", "[jobpool]")
{
  auto gate = make_rcp<GateTask>();
  std::vector<rcp<CountTask>> queued;
  {
    JobPool pool(1);
    pool.submit(gate);
    waitStarted(gate.get());
    for (int i = 0; i < 10; i++)
    {
      queued.push_back(make_rcp<CountTask>());
      pool.submit(queued.back());
    }
    gate->release = true;
  }
  CHECK(gate->state() == JobTask::State::done);
  for (auto& task : queued)
  {
    CHECK(
        (task->state() == JobTask::State::cancelled ||
         task->state() == JobTask::State::done));
    CHECK(task->runs <= 1);
  }
}

TEST_CASE(
    "pool threads and joiners claim each task exactly once under load",
    "[jobpool][stress]")
{
  for (JobSchedule schedule : kSchedules)
  {
    JobPool pool(8, schedule, 11);
    std::vector<rcp<CountTask>> tasks;
    for (int round = 0; round < 50; round++)
    {
      size_t first = tasks.size();
      for (int i = 0; i < 200; i++)
      {
        tasks.push_back(make_rcp<CountTask>());
        pool.submit(tasks.back());
      }
      std::vector<JobTask*> batch;
      for (size_t i = first; i < tasks.size(); i++)
      {
        batch.push_back(tasks[i].get());
      }
      // Drain with joinAny so the joiner claims while threads run.
      size_t remaining = batch.size();
      while (remaining > 0)
      {
        int index = pool.joinAny(batch, INFINITY);
        REQUIRE(index >= 0);
        batch.erase(batch.begin() + index);
        remaining--;
      }
    }
    for (auto& task : tasks)
    {
      REQUIRE(task->runs == 1);
    }
  }
}
