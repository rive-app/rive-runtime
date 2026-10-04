#ifndef _RIVE_WRITE_ATTRIBUTION_HPP_
#define _RIVE_WRITE_ATTRIBUTION_HPP_

#ifdef WITH_RIVE_TOOLS

#include <cstdint>
#include <vector>

struct lua_State;

namespace rive
{
class DataBind;
class StateMachineInstance;
class StateMachineLayer;
class StateMachineListener;
class WasmScriptingVM;

// What is writing view model values right now, innermost last, so a debugger
// stopped on a change can say where it came from. Tools only, and recorded
// only while a debugger has enabled it.
struct WriteSource
{
    enum class Kind : uint8_t
    {
        luau,
        wasm,
        layer,
        listener,
        dataBind,
    };
    Kind kind;
    // lua_State, WasmScriptingVM, StateMachineLayer,
    // StateMachineListener or DataBind, by kind.
    const void* object;
    const StateMachineInstance* machine;
};

class WriteAttribution
{
public:
    static bool enabled() { return s_enabled; }
    static void enable(bool enabled) { s_enabled = enabled; }
    static const std::vector<WriteSource>& sources() { return s_sources; }
    // At a frame boundary, where no scope is open.
    static void reset() { s_sources.clear(); }

private:
    friend class WriteAttributionScope;
    static bool s_enabled;
    static thread_local std::vector<WriteSource> s_sources;
};

class WriteAttributionScope
{
public:
    WriteAttributionScope(WriteSource::Kind kind,
                          const void* object,
                          const StateMachineInstance* machine = nullptr) :
        m_pushed(WriteAttribution::s_enabled),
        m_depth(WriteAttribution::s_sources.size())
    {
        if (m_pushed)
        {
            WriteAttribution::s_sources.push_back({kind, object, machine});
        }
    }
    // Back to this scope's depth rather than one pop: a Luau error longjmps
    // past the destructors of the scopes inside it.
    ~WriteAttributionScope()
    {
        if (m_pushed)
        {
            WriteAttribution::s_sources.resize(m_depth);
        }
    }
    WriteAttributionScope(const WriteAttributionScope&) = delete;
    WriteAttributionScope& operator=(const WriteAttributionScope&) = delete;

private:
    bool m_pushed;
    size_t m_depth;
};

} // namespace rive

#define RIVE_WRITE_SOURCE_CAT2(a, b) a##b
#define RIVE_WRITE_SOURCE_CAT(a, b) RIVE_WRITE_SOURCE_CAT2(a, b)
#define RIVE_WRITE_SOURCE(kind, ...)                                           \
    rive::WriteAttributionScope RIVE_WRITE_SOURCE_CAT(                         \
        riveWriteSource,                                                       \
        __LINE__)(rive::WriteSource::Kind::kind, __VA_ARGS__)

#else

#define RIVE_WRITE_SOURCE(kind, ...)

#endif
#endif
