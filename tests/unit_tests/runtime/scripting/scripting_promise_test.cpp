
#include "catch.hpp"
#include "scripting_test_utilities.hpp"

using namespace rive;

// ============================================================================
// Promise.resolve / Promise.reject
// ============================================================================

TEST_CASE("Promise.resolve creates a fulfilled promise", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local p = Promise.resolve(42)
        local result = 0
        p:andThen(function(v)
            result = v
        end)
        return result
    )");
    CHECK(lua_tonumber(test.state(), -1) == 42.0);
}

TEST_CASE("Promise.reject creates a rejected promise", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local p = Promise.reject("oops")
        local caught = ""
        p:catch(function(err)
            caught = err
        end)
        return caught
    )");
    CHECK(std::string(lua_tostring(test.state(), -1)) == "oops");
}

TEST_CASE("Promise.reject with no args uses default reason",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local p = Promise.reject()
        local caught = ""
        p:catch(function(err)
            caught = err
        end)
        return caught
    )");
    CHECK(std::string(lua_tostring(test.state(), -1)) == "rejected");
}

// ============================================================================
// andThen chaining
// ============================================================================

TEST_CASE("andThen chains propagate values", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local result = 0
        Promise.resolve(10)
            :andThen(function(v) return v * 2 end)
            :andThen(function(v) result = v end)
        return result
    )");
    CHECK(lua_tonumber(test.state(), -1) == 20.0);
}

TEST_CASE("andThen propagates to catch on rejection", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local caught = ""
        Promise.reject("fail")
            :andThen(function(v) return "should not happen" end)
            :catch(function(err) caught = err end)
        return caught
    )");
    CHECK(std::string(lua_tostring(test.state(), -1)) == "fail");
}

// ============================================================================
// finally
// ============================================================================

TEST_CASE("finally runs on fulfillment", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local ran = false
        Promise.resolve(1):finally(function()
            ran = true
        end)
        return ran
    )");
    CHECK(lua_toboolean(test.state(), -1) != 0);
}

TEST_CASE("finally runs on rejection", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local ran = false
        Promise.reject("err"):finally(function()
            ran = true
        end)
        return ran
    )");
    CHECK(lua_toboolean(test.state(), -1) != 0);
}

// ============================================================================
// Promise.all
// ============================================================================

TEST_CASE("Promise.all resolves when all resolve", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local result = 0
        Promise.all({
            Promise.resolve(1),
            Promise.resolve(2),
            Promise.resolve(3),
        }):andThen(function(values)
            result = values[1] + values[2] + values[3]
        end)
        return result
    )");
    CHECK(lua_tonumber(test.state(), -1) == 6.0);
}

TEST_CASE("Promise.all rejects if any rejects", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local caught = ""
        Promise.all({
            Promise.resolve(1),
            Promise.reject("boom"),
            Promise.resolve(3),
        }):catch(function(err)
            caught = err
        end)
        return caught
    )");
    CHECK(std::string(lua_tostring(test.state(), -1)) == "boom");
}

TEST_CASE("Promise.all with empty array resolves immediately",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local result = nil
        Promise.all({}):andThen(function(values)
            result = #values
        end)
        return result
    )");
    CHECK(lua_tonumber(test.state(), -1) == 0.0);
}

// ============================================================================
// async / await
// ============================================================================

TEST_CASE("async/await with already-resolved promise", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local result = 0
        async(function()
            local ok, v = await(Promise.resolve(99))
            result = v
        end)
        return result
    )");
    CHECK(lua_tonumber(test.state(), -1) == 99.0);
}

TEST_CASE("async/await chains multiple awaits", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local result = 0
        async(function()
            local ok1, a = await(Promise.resolve(10))
            local ok2, b = await(Promise.resolve(20))
            result = a + b
        end)
        return result
    )");
    CHECK(lua_tonumber(test.state(), -1) == 30.0);
}

TEST_CASE("await returns (false, error) on rejection — no throw",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local gotOk = nil
        local gotErr = ""
        async(function()
            local ok, err = await(Promise.reject("async_error"))
            gotOk = ok
            gotErr = err
        end)
        return tostring(gotOk) .. "," .. gotErr
    )");
    std::string result = lua_tostring(test.state(), -1);
    CHECK(result.find("false") != std::string::npos);
    CHECK(result.find("async_error") != std::string::npos);
}

TEST_CASE("await rejection resumes coroutine so cleanup code runs",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local cleanupRan = false
        local caughtErr = ""
        local p = Promise.new(function(resolve, reject)
            reject("deferred_fail")
        end)
        async(function()
            local ok, err = await(p)
            caughtErr = tostring(err)
            cleanupRan = true
        end)
        return tostring(cleanupRan) .. "," .. caughtErr
    )");
    std::string result = lua_tostring(test.state(), -1);
    CHECK(result.find("true") != std::string::npos);
    CHECK(result.find("deferred_fail") != std::string::npos);
}

TEST_CASE("await on cancelled promise returns (false, message)",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local gotOk = nil
        local gotMsg = ""
        local p = Promise.new(function() end)
        p:cancel()
        async(function()
            local ok, msg = await(p)
            gotOk = ok
            gotMsg = msg
        end)
        return tostring(gotOk) .. "," .. gotMsg
    )");
    std::string result = lua_tostring(test.state(), -1);
    CHECK(result.find("false") != std::string::npos);
    CHECK(result.find("cancelled") != std::string::npos);
}

TEST_CASE("async returns a promise that resolves with return value",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local result = 0
        local p = async(function()
            return 42
        end)
        p:andThen(function(v)
            result = v
        end)
        return result
    )");
    CHECK(lua_tonumber(test.state(), -1) == 42.0);
}

// ============================================================================
// Error handling
// ============================================================================

TEST_CASE("andThen handler error rejects chained promise",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local caught = ""
        Promise.resolve(1)
            :andThen(function(v)
                error("handler_error")
            end)
            :catch(function(err)
                caught = tostring(err)
            end)
        return caught
    )");
    // The error message should contain "handler_error"
    std::string caught = lua_tostring(test.state(), -1);
    CHECK(caught.find("handler_error") != std::string::npos);
}

TEST_CASE("finally error rejects chained promise", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local caught = ""
        Promise.resolve(1)
            :finally(function()
                error("finally_boom")
            end)
            :catch(function(err)
                caught = tostring(err)
            end)
        return caught
    )");
    std::string caught = lua_tostring(test.state(), -1);
    CHECK(caught.find("finally_boom") != std::string::npos);
}

TEST_CASE("cancelled promise does not fire andThen/catch/finally handlers",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local thenRan = false
        local catchRan = false
        local finallyRan = false

        local p = Promise.new(function(resolve, reject, onCancel)
            onCancel(function() end)
        end)

        p:andThen(function(v)
            thenRan = true
        end):catch(function(e)
            catchRan = true
        end)

        p:finally(function()
            finallyRan = true
        end)

        p:cancel()

        return tostring(thenRan) .. "," .. tostring(catchRan) .. "," .. tostring(finallyRan)
    )");
    CHECK(std::string(lua_tostring(test.state(), -1)) == "false,false,false");
}

// ============================================================================
// Promise.new
// ============================================================================

TEST_CASE("Promise.new creates a pending promise", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local p = Promise.new(function(resolve, reject, onCancel) end)
        return p:getStatus()
    )");
    CHECK(std::string(lua_tostring(test.state(), -1)) == "Pending");
}

TEST_CASE("Promise.new resolve settles the promise", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local result = 0
        Promise.new(function(resolve, reject, onCancel)
            resolve(42)
        end):andThen(function(v)
            result = v
        end)
        return result
    )");
    CHECK(lua_tonumber(test.state(), -1) == 42.0);
}

TEST_CASE("Promise.new reject settles the promise", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local caught = ""
        Promise.new(function(resolve, reject, onCancel)
            reject("oops")
        end):catch(function(err)
            caught = err
        end)
        return caught
    )");
    CHECK(std::string(lua_tostring(test.state(), -1)) == "oops");
}

TEST_CASE("Promise.new executor error rejects the promise",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local caught = ""
        Promise.new(function(resolve, reject, onCancel)
            error("executor_boom")
        end):catch(function(err)
            caught = tostring(err)
        end)
        return caught
    )");
    std::string caught = lua_tostring(test.state(), -1);
    CHECK(caught.find("executor_boom") != std::string::npos);
}

// ============================================================================
// Cancellation
// ============================================================================

TEST_CASE("cancel sets state to Cancelled", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local p = Promise.new(function(resolve, reject, onCancel) end)
        p:cancel()
        return p:getStatus()
    )");
    CHECK(std::string(lua_tostring(test.state(), -1)) == "Cancelled");
}

TEST_CASE("cancel fires onCancel hook", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local hookFired = false
        local p = Promise.new(function(resolve, reject, onCancel)
            onCancel(function()
                hookFired = true
            end)
        end)
        p:cancel()
        return hookFired
    )");
    CHECK(lua_toboolean(test.state(), -1) != 0);
}

TEST_CASE("cancel propagates down to consumers", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local p = Promise.new(function(resolve, reject, onCancel) end)
        local child = p:andThen(function() end)
        p:cancel()
        return child:getStatus()
    )");
    CHECK(std::string(lua_tostring(test.state(), -1)) == "Cancelled");
}

TEST_CASE("cancel propagates up when all consumers cancelled",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local p = Promise.new(function(resolve, reject, onCancel) end)
        local c1 = p:andThen(function() end)
        local c2 = p:andThen(function() end)
        c1:cancel()
        local afterFirst = p:getStatus()
        c2:cancel()
        local afterSecond = p:getStatus()
        return afterFirst .. "," .. afterSecond
    )");
    CHECK(std::string(lua_tostring(test.state(), -1)) == "Pending,Cancelled");
}

TEST_CASE("cancel is no-op on already settled promise", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local p = Promise.resolve(1)
        p:cancel()
        return p:getStatus()
    )");
    CHECK(std::string(lua_tostring(test.state(), -1)) == "Fulfilled");
}

TEST_CASE("cancelled promise does not fire andThen callbacks",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local called = false
        local p = Promise.new(function(resolve, reject, onCancel) end)
        p:andThen(function() called = true end)
        p:cancel()
        return called
    )");
    CHECK(lua_toboolean(test.state(), -1) == 0);
}

TEST_CASE("getStatus returns correct strings", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local pending = Promise.new(function() end):getStatus()
        local fulfilled = Promise.resolve(1):getStatus()
        local rejected = Promise.reject("e"):getStatus()
        local cancelled = Promise.new(function() end)
        cancelled:cancel()
        local cancelledStatus = cancelled:getStatus()
        return pending .. "," .. fulfilled .. "," .. rejected .. "," .. cancelledStatus
    )");
    CHECK(std::string(lua_tostring(test.state(), -1)) ==
          "Pending,Fulfilled,Rejected,Cancelled");
}

TEST_CASE("await on already-rejected promise returns (false, error)",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local gotOk = nil
        local gotErr = ""
        async(function()
            local ok, err = await(Promise.reject("already_rejected"))
            gotOk = ok
            gotErr = err
        end)
        return tostring(gotOk) .. "," .. gotErr
    )");
    std::string result = lua_tostring(test.state(), -1);
    CHECK(result.find("false") != std::string::npos);
    CHECK(result.find("already_rejected") != std::string::npos);
}

TEST_CASE("Promise.all cancels remaining on rejection", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local hookFired = false
        local p1 = Promise.new(function(resolve, reject, onCancel)
            onCancel(function() hookFired = true end)
        end)
        local p2 = Promise.reject("boom")

        local caught = ""
        Promise.all({p1, p2}):catch(function(err)
            caught = err
        end)
        return caught .. "," .. tostring(hookFired)
    )");
    CHECK(std::string(lua_tostring(test.state(), -1)) == "boom,true");
}

// ============================================================================
// Non-throwing await() coverage
// ============================================================================

TEST_CASE("await success returns (true, value) explicitly",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local gotOk = nil
        local gotVal = nil
        async(function()
            local ok, val = await(Promise.resolve(42))
            gotOk = ok
            gotVal = val
        end)
        return tostring(gotOk) .. "," .. tostring(gotVal)
    )");
    CHECK(std::string(lua_tostring(test.state(), -1)) == "true,42");
}

TEST_CASE("await mixed success and failure in sequence", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local results = {}
        async(function()
            local ok1, v1 = await(Promise.resolve("good"))
            table.insert(results, tostring(ok1) .. ":" .. tostring(v1))

            local ok2, v2 = await(Promise.reject("bad"))
            table.insert(results, tostring(ok2) .. ":" .. tostring(v2))

            local ok3, v3 = await(Promise.resolve("recovered"))
            table.insert(results, tostring(ok3) .. ":" .. tostring(v3))
        end)
        return table.concat(results, ",")
    )");
    std::string result = lua_tostring(test.state(), -1);
    CHECK(result == "true:good,false:bad,true:recovered");
}

TEST_CASE("async function handles rejection and returns recovery value",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local final = 0
        local p = async(function()
            local ok, err = await(Promise.reject("oops"))
            if not ok then
                return 999  -- recovery value
            end
            return 0
        end)
        p:andThen(function(v)
            final = v
        end)
        return final
    )");
    CHECK(lua_tonumber(test.state(), -1) == 999.0);
}

TEST_CASE("await in retry loop pattern", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local attempts = 0
        local finalVal = nil
        async(function()
            for i = 1, 3 do
                attempts = i
                local p
                if i < 3 then
                    p = Promise.reject("retry")
                else
                    p = Promise.resolve("done")
                end
                local ok, val = await(p)
                if ok then
                    finalVal = val
                    break
                end
            end
        end)
        return tostring(attempts) .. "," .. tostring(finalVal)
    )");
    CHECK(std::string(lua_tostring(test.state(), -1)) == "3,done");
}

TEST_CASE("onCancel via instance method", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local hookFired = false
        local p = Promise.new(function() end)
        p:onCancel(function()
            hookFired = true
        end)
        p:cancel()
        return hookFired
    )");
    CHECK(lua_toboolean(test.state(), -1) != 0);
}

// ============================================================================
// Promise/A+ flattening (resolve adopts inner promise state)
// ============================================================================

TEST_CASE("Promise.resolve flattens a fulfilled promise",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local result = 0
        local inner = Promise.resolve(42)
        local outer = Promise.resolve(inner)
        outer:andThen(function(v) result = v end)
        return result
    )");
    CHECK(lua_tonumber(test.state(), -1) == 42.0);
}

TEST_CASE("Promise.resolve flattens a rejected promise", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local caught = ""
        local inner = Promise.reject("boom")
        local outer = Promise.resolve(inner)
        outer:catch(function(err) caught = err end)
        return caught
    )");
    CHECK(std::string(lua_tostring(test.state(), -1)) == "boom");
}

TEST_CASE("andThen flattens a returned promise", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local result = 0
        Promise.resolve(10)
            :andThen(function(v)
                return Promise.resolve(v * 3)
            end)
            :andThen(function(v) result = v end)
        return result
    )");
    CHECK(lua_tonumber(test.state(), -1) == 30.0);
}

TEST_CASE("andThen flattens a returned rejected promise",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local caught = ""
        Promise.resolve(1)
            :andThen(function(v)
                return Promise.reject("handler-fail")
            end)
            :catch(function(err) caught = err end)
        return caught
    )");
    CHECK(std::string(lua_tostring(test.state(), -1)) == "handler-fail");
}

TEST_CASE("recursive flattening through multiple promise layers",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local result = 0
        local p = Promise.resolve(Promise.resolve(Promise.resolve(99)))
        p:andThen(function(v) result = v end)
        return result
    )");
    CHECK(lua_tonumber(test.state(), -1) == 99.0);
}

TEST_CASE("Promise.new resolve callback flattens a promise",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local result = 0
        local p = Promise.new(function(resolve)
            resolve(Promise.resolve(77))
        end)
        p:andThen(function(v) result = v end)
        return result
    )");
    CHECK(lua_tonumber(test.state(), -1) == 77.0);
}

TEST_CASE("catch handler returning a promise flattens for recovery",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local result = 0
        Promise.reject("err")
            :catch(function(e)
                return Promise.resolve(55)
            end)
            :andThen(function(v) result = v end)
        return result
    )");
    CHECK(lua_tonumber(test.state(), -1) == 55.0);
}

TEST_CASE("flattening a pending promise adopts its eventual value",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local result = 0
        local innerResolve
        local inner = Promise.new(function(resolve)
            innerResolve = resolve
        end)
        -- Outer resolves with a pending inner promise.
        local outer = Promise.new(function(resolve)
            resolve(inner)
        end)
        outer:andThen(function(v) result = v end)
        -- At this point outer should be pending (adopted inner).
        -- Now settle the inner promise.
        innerResolve(123)
        return result
    )");
    CHECK(lua_tonumber(test.state(), -1) == 123.0);
}

TEST_CASE("flattening a pending promise that rejects", "[scripting][promise]")
{
    ScriptingTest test(R"(
        local caught = ""
        local innerReject
        local inner = Promise.new(function(resolve, reject)
            innerReject = reject
        end)
        local outer = Promise.new(function(resolve)
            resolve(inner)
        end)
        outer:catch(function(err) caught = err end)
        innerReject("deferred-fail")
        return caught
    )");
    CHECK(std::string(lua_tostring(test.state(), -1)) == "deferred-fail");
}

TEST_CASE("cancelling adopted promise propagates to inner",
          "[scripting][promise]")
{
    ScriptingTest test(R"(
        local innerCancelled = false
        local inner = Promise.new(function() end)
        inner:onCancel(function() innerCancelled = true end)
        local outer = Promise.new(function(resolve)
            resolve(inner)
        end)
        outer:cancel()
        return innerCancelled
    )");
    CHECK(lua_toboolean(test.state(), -1) != 0);
}
