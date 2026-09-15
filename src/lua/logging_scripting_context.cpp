/*
 * Copyright 2025 Rive
 */

#include "rive/logging_scripting_context.hpp"

#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"

#include <cstring>
#include <string>
#include <utility>

namespace rive
{
namespace
{
// Routes a script's console output and errors to a host-provided sink instead
// of stdout/stderr. print() may be called several times per line, so text is
// accumulated between printBeginLine()/printEndLine() and flushed as one line.
class LoggingScriptingContext : public CPPRuntimeScriptingContext
{
public:
    LoggingScriptingContext(Factory* factory, ScriptingLogSink sink) :
        CPPRuntimeScriptingContext(factory), m_sink(std::move(sink))
    {}

    void printBeginLine(lua_State*) override { m_line.clear(); }

    void print(Span<const char> data) override
    {
        m_line.append(data.data(), data.size());
    }

    void printEndLine() override
    {
        m_sink(ScriptingLogLevel::info, m_line.c_str(), m_line.size());
        m_line.clear();
    }

    void printError(lua_State* state) override
    {
        const char* error = lua_tostring(state, -1);
        if (error != nullptr)
        {
            m_sink(ScriptingLogLevel::error, error, std::strlen(error));
        }
    }

private:
    ScriptingLogSink m_sink;
    std::string m_line;
};
} // namespace

ScriptingContextFactory makeLoggingScriptingContextFactory(
    ScriptingLogSink sink)
{
    if (!sink)
    {
        return nullptr;
    }
    return [sink = std::move(sink)](
               Factory* factory) -> std::unique_ptr<ScriptingContext> {
        return std::make_unique<LoggingScriptingContext>(factory, sink);
    };
}
} // namespace rive

#else // !WITH_RIVE_SCRIPTING

namespace rive
{
ScriptingContextFactory makeLoggingScriptingContextFactory(ScriptingLogSink)
{
    return nullptr;
}
} // namespace rive

#endif
