#include <catch.hpp>
#include <functional>
#include "rive/component_dirt.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_bind_container.hpp"
#include "rive/data_bind_flags.hpp"

using namespace rive;

namespace
{

class TestContainer : public DataBindContainer
{
public:
    using DataBindContainer::deleteDataBinds;
};

class TestDataBind : public DataBind
{
public:
    int updateCalls = 0;
    int updateSourceBindingCalls = 0;
    std::function<void()> onUpdate;

    void update(ComponentDirt value) override
    {
        updateCalls++;
        if (onUpdate)
        {
            onUpdate();
        }
    }
    void updateSourceBinding(bool invalidate = false) override
    {
        updateSourceBindingCalls++;
    }
};

TestDataBind* makeBind(DataBindFlags f = DataBindFlags::ToTarget)
{
    auto* b = new TestDataBind();
    b->flags(static_cast<uint32_t>(f));
    return b;
}

} // namespace

TEST_CASE("addDataBind registers backpointer and persisting flag",
          "[data_bind_container]")
{
    TestContainer c;
    auto* toTarget = makeBind(DataBindFlags::ToTarget);
    auto* toSource = makeBind(DataBindFlags::ToSource);

    c.addDataBind(toTarget);
    c.addDataBind(toSource);

    REQUIRE(toTarget->m_container == &c);
    REQUIRE(toSource->m_container == &c);
    REQUIRE(toTarget->inPersistingList() == false);
    REQUIRE(toSource->inPersistingList() == true);

    c.deleteDataBinds();
}

TEST_CASE("removeDataBind clears backpointer and membership flags",
          "[data_bind_container]")
{
    TestContainer c;
    auto* b = makeBind(DataBindFlags::ToSource);
    c.addDataBind(b);
    c.addDirtyDataBind(b); // no-op for toSource, but exercises the path

    REQUIRE(b->m_container == &c);
    REQUIRE(b->inPersistingList() == true);

    c.removeDataBind(b);

    REQUIRE(b->m_container == nullptr);
    REQUIRE(b->inPersistingList() == false);
    REQUIRE(b->inDirtyList() == false);

    delete b;
}

TEST_CASE("addDirtyDataBind dedupes via inDirtyList flag",
          "[data_bind_container]")
{
    TestContainer c;
    auto* b = makeBind(DataBindFlags::ToTarget);
    c.addDataBind(b);
    b->dirt(ComponentDirt::Bindings);

    REQUIRE(b->inDirtyList() == false);
    c.addDirtyDataBind(b);
    REQUIRE(b->inDirtyList() == true);

    // Spam-add — should all early-return on the flag.
    for (int i = 0; i < 25; i++)
    {
        c.addDirtyDataBind(b);
    }

    c.updateDataBinds();

    // If dedupe were broken, update() would have been called once per entry.
    REQUIRE(b->updateCalls == 1);
    REQUIRE(b->inDirtyList() == false);

    c.deleteDataBinds();
}

TEST_CASE("recursive updateDataBinds is rejected", "[data_bind_container]")
{
    TestContainer c;
    auto* b = makeBind(DataBindFlags::ToSource);
    c.addDataBind(b);
    b->dirt(ComponentDirt::Bindings);

    // External flag — must NOT self-clear b->onUpdate inside the lambda;
    // doing so destroys the std::function storage holding the running
    // lambda and leaves `this` dangling for the rest of the call.
    bool fired = false;
    b->onUpdate = [&]() {
        if (fired)
        {
            return;
        }
        fired = true;
        c.updateDataBinds();
    };

    c.updateDataBinds();

    // Outer call ran update() once; the inner recursive call must have
    // early-returned without touching the loops.
    REQUIRE(b->updateCalls == 1);

    c.deleteDataBinds();
}

TEST_CASE("addDataBind during update is deferred and flushed after",
          "[data_bind_container]")
{
    TestContainer c;
    auto* b = makeBind(DataBindFlags::ToSource);
    auto* added = makeBind(DataBindFlags::ToSource);
    c.addDataBind(b);
    b->dirt(ComponentDirt::Bindings);

    bool fired = false;
    b->onUpdate = [&]() {
        if (fired)
        {
            return;
        }
        fired = true;
        c.addDataBind(added);
        // Defer means the new bind is NOT live yet during outer iteration.
        REQUIRE(added->m_container == nullptr);
        REQUIRE(added->inPersistingList() == false);
    };

    c.updateDataBinds();

    // Flush completed after the loops ran — the deferred add lands now.
    REQUIRE(added->m_container == &c);
    REQUIRE(added->inPersistingList() == true);
    // It was added post-iteration, so its update() should not run this tick.
    REQUIRE(added->updateCalls == 0);
    REQUIRE(b->updateCalls == 1);

    c.deleteDataBinds();
}

TEST_CASE("removeDataBind during update is deferred and flushed after",
          "[data_bind_container]")
{
    TestContainer c;
    auto* b = makeBind(DataBindFlags::ToSource);
    auto* toRemove = makeBind(DataBindFlags::ToSource);
    c.addDataBind(b);
    c.addDataBind(toRemove);
    b->dirt(ComponentDirt::Bindings);

    bool fired = false;
    b->onUpdate = [&]() {
        if (fired)
        {
            return;
        }
        fired = true;
        c.removeDataBind(toRemove);
        // Defer means toRemove stays in the lists until the flush; if the
        // removal happened inline, m_persistingDataBinds erase would
        // invalidate the active range-for iterator (crash) and the
        // backpointer would already be null here.
        REQUIRE(toRemove->m_container == &c);
        REQUIRE(toRemove->inPersistingList() == true);
    };

    c.updateDataBinds();

    REQUIRE(toRemove->m_container == nullptr);
    REQUIRE(toRemove->inPersistingList() == false);

    c.deleteDataBinds();
    delete toRemove;
}

TEST_CASE("same-tick add then remove of the same bind resolves to removed",
          "[data_bind_container]")
{
    TestContainer c;
    auto* b = makeBind(DataBindFlags::ToSource);
    auto* transient = makeBind(DataBindFlags::ToSource);
    c.addDataBind(b);
    b->dirt(ComponentDirt::Bindings);

    bool fired = false;
    b->onUpdate = [&]() {
        if (fired)
        {
            return;
        }
        fired = true;
        c.addDataBind(transient);    // queued in pendingAdditions
        c.removeDataBind(transient); // queued in pendingRemovals
    };

    c.updateDataBinds();

    // Additions flush before removals, so the chronological add-then-remove
    // resolves to: bind ends up removed.
    REQUIRE(transient->m_container == nullptr);
    REQUIRE(transient->inPersistingList() == false);

    c.deleteDataBinds();
    delete transient;
}

TEST_CASE("pending-dirty binds swap into dirty on next tick",
          "[data_bind_container]")
{
    TestContainer c;
    auto* driver = makeBind(DataBindFlags::ToSource);
    auto* dependent = makeBind(DataBindFlags::ToTarget);
    c.addDataBind(driver);
    c.addDataBind(dependent);
    driver->dirt(ComponentDirt::Bindings);

    bool fired = false;
    driver->onUpdate = [&]() {
        if (fired)
        {
            return;
        }
        fired = true;
        // Marking another bind dirty while processing — should land in
        // m_pendingDirtyDataBinds, not m_dirtyDataBinds.
        dependent->dirt(ComponentDirt::Bindings);
        c.addDirtyDataBind(dependent);
        REQUIRE(dependent->inDirtyList() == true);
    };

    c.updateDataBinds();

    // Dependent was queued mid-processing — should not have been updated yet.
    REQUIRE(dependent->updateCalls == 0);
    // But should still be flagged for next tick (lives in dirty list now).
    REQUIRE(dependent->inDirtyList() == true);

    c.updateDataBinds();

    // Second tick — dependent is in the dirty list, gets processed once,
    // flag cleared.
    REQUIRE(dependent->updateCalls == 1);
    REQUIRE(dependent->inDirtyList() == false);

    c.deleteDataBinds();
}

TEST_CASE("source-originated dirt does not run target->source",
          "[data_bind_container]")
{
    // A non-persisting bind carrying only source-flavored dirt must NOT trigger
    // the target->source apply — otherwise (once a converter's Dependents dirt
    // invalidates the cached value) it clobbers the source's fresh value with
    // the stale target before update() propagates it.
    TestContainer c;
    auto* b = makeBind(DataBindFlags::ToTarget);
    c.addDataBind(b);
    REQUIRE(b->inPersistingList() == false);
    b->dirt(ComponentDirt::Bindings);
    c.addDirtyDataBind(b);

    c.updateDataBinds();

    CHECK(b->updateCalls == 1);
    CHECK(b->updateSourceBindingCalls == 0);

    // Same, but for the interpolator-driven frame shape that markConverterDirty
    // produces (Bindings | Dependents) — the Dependents bit must not re-open
    // the target->source path for a source-originated change.
    b->dirt(ComponentDirt::Bindings | ComponentDirt::Dependents);
    c.addDirtyDataBind(b);
    c.updateDataBinds();
    CHECK(b->updateSourceBindingCalls == 0);

    c.deleteDataBinds();
}

TEST_CASE("target-originated dirt runs target->source", "[data_bind_container]")
{
    TestContainer c;
    auto* b = makeBind(DataBindFlags::ToTarget);
    c.addDataBind(b);
    b->dirt(ComponentDirt::BindingsTarget);
    c.addDirtyDataBind(b);

    c.updateDataBinds();

    CHECK(b->updateSourceBindingCalls == 1);

    c.deleteDataBinds();
}

TEST_CASE("persisting bind polls target->source regardless of origin",
          "[data_bind_container]")
{
    // Polled (non-push) toSource targets never receive BindingsTarget, so the
    // gate must still run target->source for them every frame.
    TestContainer c;
    auto* b = makeBind(DataBindFlags::ToSource);
    c.addDataBind(b);
    REQUIRE(b->inPersistingList() == true);
    b->dirt(ComponentDirt::Bindings);

    c.updateDataBinds();

    CHECK(b->updateSourceBindingCalls == 1);

    c.deleteDataBinds();
}

TEST_CASE("addDirt latches a single change origin", "[data_bind_container]")
{
    auto* b = makeBind(DataBindFlags::ToTarget);
    b->addDirt(ComponentDirt::Bindings, false);
    CHECK(b->targetOrigin() == false);
    b->addDirt(ComponentDirt::BindingsTarget, false);
    CHECK(b->targetOrigin() == true);
    // Suppressed self-notify must not flip the latched origin.
    b->suppressDirt(true);
    b->addDirt(ComponentDirt::Bindings, false);
    CHECK(b->targetOrigin() == true);
    b->suppressDirt(false);
    delete b;

    // Reconcile marks both bits — the favored direction wins the origin.
    auto* favorTarget = makeBind(DataBindFlags::TwoWay);
    favorTarget->addDirt(ComponentDirt::Bindings |
                             ComponentDirt::BindingsTarget,
                         false);
    CHECK(favorTarget->targetOrigin() == true);
    delete favorTarget;

    auto* favorSource = makeBind(DataBindFlags::TwoWay |
                                 DataBindFlags::SourceToTargetRunsFirst);
    favorSource->addDirt(ComponentDirt::Bindings |
                             ComponentDirt::BindingsTarget,
                         false);
    CHECK(favorSource->targetOrigin() == false);
    delete favorSource;
}
