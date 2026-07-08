/*
 * Copyright 2025 Rive
 */

#ifndef _RIVE_LOGGING_SCRIPTING_CONTEXT_HPP_
#define _RIVE_LOGGING_SCRIPTING_CONTEXT_HPP_

#include "rive/command_queue.hpp"

#include <cstddef>
#include <functional>

// A lua-free bridge for routing a script's console/error output to a host
// logger.

namespace rive
{
enum class ScriptingLogLevel
{
    info,
    warn,
    error,
};

// Receives a single fully-assembled UTF-8 log line from a script. The line has
// no trailing newline. Invoked on the command server thread. `data` is only
// valid for the duration of the call (copy it if you need to retain it).
using ScriptingLogSink = std::function<
    void(ScriptingLogLevel level, const char* data, size_t length)>;

// Builds a ScriptingContextFactory whose contexts forward all script console
// output as ScriptingLogLevel::info lines and runtime/Lua errors as
// ScriptingLogLevel::error lines to `sink`.
ScriptingContextFactory makeLoggingScriptingContextFactory(
    ScriptingLogSink sink);
} // namespace rive

#endif
