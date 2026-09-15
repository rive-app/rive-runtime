#ifdef WITH_RIVE_SCRIPTING
// Scoped asset reference resolution, shared between the Luau backend and the
// wasm module blob (which cannot link rive_lua_libs.cpp).
#include "rive/lua/rive_lua_libs.hpp"

#include "lua.h"

using namespace rive;

ScriptingContext::ScopedAssetReference::ScopedAssetReference(
    lua_State* L,
    const char* reference)
{
    std::string request(reference);
    static const std::string kLibraryPrefix = "lib:";
    if (request.compare(0, kLibraryPrefix.size(), kLibraryPrefix) == 0)
    {
        size_t slash = request.find('/', kLibraryPrefix.size());
        if (slash != std::string::npos)
        {
            m_label = request.substr(kLibraryPrefix.size(),
                                     slash - kLibraryPrefix.size());
            m_path = request.substr(slash + 1);
            return;
        }
    }
    m_bare = std::move(request);
    if (L == nullptr)
    {
        return;
    }
    // Mangled chunknames self-describe; the calling chunk's first segment
    // carries its scope. Skip C frames like lua_require does.
    lua_Debug ar;
    int level = 1;
    do
    {
        if (!lua_getinfo(L, level++, "s", &ar))
        {
            return;
        }
    } while (ar.what[0] == 'C');
    if (ar.source == nullptr)
    {
        return;
    }
    std::string chunkname(ar.source);
    size_t slash = chunkname.find('/');
    if (slash == std::string::npos)
    {
        return;
    }
    size_t at = chunkname.find('@');
    if (at > 0 && at < slash)
    {
        m_scopePrefix = chunkname.substr(0, slash);
    }
}

bool ScriptingContext::ScopedAssetReference::matchesLibrary(
    const std::string& registeredName) const
{
    // <label>[#<digits>]@<digits>/<path>
    if (registeredName.size() <= m_label.size() ||
        registeredName.compare(0, m_label.size(), m_label) != 0)
    {
        return false;
    }
    size_t i = m_label.size();
    if (registeredName[i] == '#')
    {
        size_t start = ++i;
        while (i < registeredName.size() && registeredName[i] >= '0' &&
               registeredName[i] <= '9')
        {
            i++;
        }
        if (i == start)
        {
            return false;
        }
    }
    if (i >= registeredName.size() || registeredName[i] != '@')
    {
        return false;
    }
    size_t start = ++i;
    while (i < registeredName.size() && registeredName[i] >= '0' &&
           registeredName[i] <= '9')
    {
        i++;
    }
    if (i == start || i >= registeredName.size() || registeredName[i] != '/')
    {
        return false;
    }
    return registeredName.compare(i + 1, std::string::npos, m_path) == 0;
}

int ScriptingContext::ScopedAssetReference::match(
    const std::string& registeredName,
    const std::string& shortName) const
{
    if (!m_label.empty())
    {
        return matchesLibrary(registeredName) ? 1 : 0;
    }
    if (!m_scopePrefix.empty() &&
        registeredName.size() > m_scopePrefix.size() &&
        registeredName[m_scopePrefix.size()] == '/' &&
        registeredName.compare(0, m_scopePrefix.size(), m_scopePrefix) == 0)
    {
        // Inside the caller's library both the scope relative path and the
        // short name address it.
        if (registeredName.compare(m_scopePrefix.size() + 1,
                                   std::string::npos,
                                   m_bare) == 0 ||
            shortName == m_bare)
        {
            return 2;
        }
        return 0;
    }
    // Host assets only; other libraries need lib: or their own scope.
    size_t slash = registeredName.find('/');
    size_t firstSegment =
        slash == std::string::npos ? registeredName.size() : slash;
    size_t at = registeredName.find('@');
    if (at > 0 && at < firstSegment)
    {
        return 0;
    }
    return registeredName == m_bare || shortName == m_bare ? 1 : 0;
}

#endif
