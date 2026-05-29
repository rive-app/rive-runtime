/*
 * Copyright 2026 Rive
 */

#include "rive/async/work_task.hpp"
#include <catch.hpp>

namespace rive
{

class TestWorkTask : public WorkTask
{
public:
    bool executeResult = true;

    bool execute() override
    {
        if (!executeResult)
        {
            m_errorMessage = "test failure";
        }
        return executeResult;
    }
};

TEST_CASE("WorkTask-default-state", "[WorkTask]")
{
    auto task = rcp<TestWorkTask>(new TestWorkTask());
    CHECK(task->status() == WorkStatus::Pending);
    CHECK(task->isCancelled() == false);
    CHECK(task->ownerId() == 0);
    CHECK(task->submitGeneration() == 0);
    CHECK(task->errorMessage().empty());
}

TEST_CASE("WorkTask-status-transitions", "[WorkTask]")
{
    auto task = rcp<TestWorkTask>(new TestWorkTask());

    task->setStatus(WorkStatus::Running);
    CHECK(task->status() == WorkStatus::Running);

    task->setStatus(WorkStatus::Completed);
    CHECK(task->status() == WorkStatus::Completed);

    task->setStatus(WorkStatus::Failed);
    CHECK(task->status() == WorkStatus::Failed);

    task->setStatus(WorkStatus::Cancelled);
    CHECK(task->status() == WorkStatus::Cancelled);
}

TEST_CASE("WorkTask-cancel", "[WorkTask]")
{
    auto task = rcp<TestWorkTask>(new TestWorkTask());
    CHECK(task->isCancelled() == false);
    task->cancel();
    CHECK(task->isCancelled() == true);
}

TEST_CASE("WorkTask-owner-and-generation", "[WorkTask]")
{
    auto task = rcp<TestWorkTask>(new TestWorkTask());

    task->setOwnerId(42);
    CHECK(task->ownerId() == 42);

    task->setSubmitGeneration(7);
    CHECK(task->submitGeneration() == 7);
}

TEST_CASE("WorkTask-execute-success", "[WorkTask]")
{
    auto task = rcp<TestWorkTask>(new TestWorkTask());
    task->executeResult = true;
    CHECK(task->execute() == true);
    CHECK(task->errorMessage().empty());
}

TEST_CASE("WorkTask-execute-failure", "[WorkTask]")
{
    auto task = rcp<TestWorkTask>(new TestWorkTask());
    task->executeResult = false;
    CHECK(task->execute() == false);
    CHECK(task->errorMessage() == "test failure");
}

} // namespace rive
