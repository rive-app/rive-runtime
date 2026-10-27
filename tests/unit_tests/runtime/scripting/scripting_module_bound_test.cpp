#include "catch.hpp"
#include "scripting_test_utilities.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive_file_reader.hpp"
#include "utils/no_op_factory.hpp"
#include <chrono>
#include <memory>
#include <string>
#include <vector>

using namespace rive;

namespace
{
class ErrorCapturingContext : public CPPRuntimeScriptingContext
{
public:
  using CPPRuntimeScriptingContext::CPPRuntimeScriptingContext;

  void printError(lua_State* state) override
  {
    const char* error = lua_tostring(state, -1);
    errors.push_back(error != nullptr ? error : "");
  }

  std::vector<std::string> errors;
};

constexpr const char* kWatchdogMessage = "test watchdog fired";
constexpr std::chrono::milliseconds kWatchdogDeadline(8000);

// The interrupt has no upvalues, so its deadline lives here.
std::chrono::steady_clock::time_point gWatchdogDeadline;

// Clears itself before raising so a later test never inherits it.
void watchdogInterrupt(lua_State* L, int gc)
{
  if (gc >= 0 || std::chrono::steady_clock::now() < gWatchdogDeadline)
  {
    return;
  }
  lua_callbacks(L)->interrupt = nullptr;
  luaL_error(L, "%s", kWatchdogMessage);
}

// Sits above the bound under test: it must only fire if that bound is absent.
struct ScopedWatchdog
{
  explicit ScopedWatchdog(lua_State* state) : L(state)
  {
    gWatchdogDeadline = std::chrono::steady_clock::now() + kWatchdogDeadline;
    lua_callbacks(L)->interrupt = watchdogInterrupt;
  }
  ~ScopedWatchdog() { lua_callbacks(L)->interrupt = nullptr; }

  lua_State* L;
};

rcp<ScriptingVM> makeVM()
{
  return make_rcp<ScriptingVM>(
      std::make_unique<ErrorCapturingContext>(&gNoOpFactory, 0));
}

bool registerSource(ScriptingVM* vm, const char* name, const char* source)
{
  size_t bytecodeSize = 0;
  char* bytecode = luau_compile(source, strlen(source), nullptr, &bytecodeSize);
  REQUIRE(bytecode != nullptr);
  bool registered = vm->registerModule(
      name,
      Span<uint8_t>((uint8_t*)bytecode, bytecodeSize));
  free(bytecode);
  return registered;
}

bool contains(const std::vector<std::string>& errors, const char* needle)
{
  for (const std::string& error : errors)
  {
    if (error.find(needle) != std::string::npos)
    {
      return true;
    }
  }
  return false;
}
} // namespace

TEST_CASE("a module top level that never terminates is stopped", "[scripting]")
{
  auto vm = makeVM();
  auto* context = static_cast<ErrorCapturingContext*>(vm->context());

  bool registered;
  std::chrono::milliseconds elapsed;
  {
    ScopedWatchdog watchdog(vm->state());
    auto start = std::chrono::steady_clock::now();
    registered = registerSource(vm.get(), "spin", "while true do end");
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
  }

  std::string captured;
  for (const std::string& error : context->errors)
  {
    captured += error + "\n";
  }
  INFO("elapsed " << elapsed.count() << "ms");
  INFO("captured errors: " << captured);
  CHECK_FALSE(registered);
  CHECK(context->budgetExceeded);
  CHECK(contains(context->errors, "execution exceeded"));
  CHECK_FALSE(contains(context->errors, kWatchdogMessage));
  CHECK(elapsed < kWatchdogDeadline);
  CHECK(elapsed.count() < 3 * ScriptingContext::moduleExecutionTimeoutMs);
}

TEST_CASE(
    "a module top level that does real work still registers",
    "[scripting]")
{
  auto vm = makeVM();
  auto* context = static_cast<ErrorCapturingContext*>(vm->context());

  const char* source = R"SRC(
local entries = {}
for i = 1, 20000 do
    entries[i] = i
end
local sum = 0
for i = 1, #entries do
    sum += entries[i]
end
return { sum = sum }
)SRC";

  auto start = std::chrono::steady_clock::now();
  bool registered = registerSource(vm.get(), "summed", source);
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);
  INFO("module initialisation took " << elapsed.count() << "ms");

  CHECK(registered);
  CHECK(context->errors.empty());

  lua_State* L = vm->state();
  const char* consumer = R"SRC(return require("summed").sum)SRC";
  size_t bytecodeSize = 0;
  char* bytecode = luau_compile(
      consumer,
      strlen(consumer),
      nullptr,
      &bytecodeSize);
  REQUIRE(bytecode != nullptr);
  REQUIRE(luau_load(L, "consumer", bytecode, bytecodeSize, 0) == LUA_OK);
  free(bytecode);
  REQUIRE(lua_pcall(L, 0, 1, 0) == LUA_OK);
  CHECK(lua_tonumber(L, -1) == 200010000.0);
  lua_pop(L, 1);
}

TEST_CASE(
    "module execution raises a host timeout to the module floor",
    "[scripting]")
{
  auto vm = makeVM();
  auto* context = static_cast<ErrorCapturingContext*>(vm->context());
  lua_State* L = vm->state();

  const int floor = ScriptingContext::moduleExecutionTimeoutMs;
  const int belowFloor = floor / 2;
  const int aboveFloor = floor * 2;
  REQUIRE(belowFloor > 0);

  context->setTimeoutMs(0);
  CHECK(context->timeoutMs() == 0);
  lua_Callbacks* callbacks = lua_callbacks(L);
  auto interruptBefore = callbacks->interrupt;
  context->startModuleExecution(L);
  CHECK(context->timeoutMs() == floor);
  CHECK(callbacks->interrupt != interruptBefore);
  context->endModuleExecution(L);
  CHECK(context->timeoutMs() == 0);
  CHECK(callbacks->interrupt == interruptBefore);

  context->setTimeoutMs(belowFloor);
  CHECK(context->timeoutMs() == belowFloor);
  context->startModuleExecution(L);
  CHECK(context->timeoutMs() == floor);
  context->endModuleExecution(L);
  CHECK(context->timeoutMs() == belowFloor);

  context->setTimeoutMs(aboveFloor);
  CHECK(context->timeoutMs() == aboveFloor);
  context->startModuleExecution(L);
  CHECK(context->timeoutMs() == aboveFloor);
  context->endModuleExecution(L);
  CHECK(context->timeoutMs() == aboveFloor);
}

TEST_CASE("a failed module top level restores the host timeout", "[scripting]")
{
  auto vm = makeVM();
  auto* context = static_cast<ErrorCapturingContext*>(vm->context());

  const int hostTimeout = ScriptingContext::moduleExecutionTimeoutMs / 4;
  REQUIRE(hostTimeout > 0);
  context->setTimeoutMs(hostTimeout);
  REQUIRE(context->timeoutMs() == hostTimeout);

  bool registered;
  {
    ScopedWatchdog watchdog(vm->state());
    registered = registerSource(vm.get(), "spin", "while true do end");
  }

  CHECK_FALSE(registered);
  CHECK(contains(context->errors, "execution exceeded"));
  CHECK_FALSE(contains(context->errors, kWatchdogMessage));
  CHECK(context->timeoutMs() == hostTimeout);
}
