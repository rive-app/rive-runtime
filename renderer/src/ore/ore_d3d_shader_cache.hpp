/*
 * Copyright 2025 Rive
 */

#pragma once

#include <cstddef>
#include <list>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace rive::ore
{

// fxc has no cache of its own: D3DCompile recompiles identical HLSL every time
// it is asked, and a scripted shader takes hundreds of milliseconds at O3. The
// script layer makes a fresh ShaderModule each time a script loads a shader
// (every runtime regeneration in the editor, every artboard instance at
// runtime), so each of those paid that cost again on the render thread. Metal
// and the browsers absorb repeats in a system cache; D3D needs its own.
//
// Keyed by everything D3DCompile reads: the target profile, the entry point
// and the normalized source.
inline std::string oreD3DShaderCacheKey(
    const char* target,
    const char* entry,
    const std::string& source)
{
  std::string key;
  key.reserve(source.size() + 32);
  key += target;
  key += ':';
  key += entry;
  key += '\n';
  key += source;
  return key;
}

// Least-recently-used, bounded by bytes. Every edit of a shader's source is a
// new key, so an editing session would otherwise keep every revision it ever
// compiled; once the budget is exceeded the oldest unused entries go. Live
// modules keep their own copies, so eviction only drops the cache's share.
// The budget counts the key (the HLSL source) and the bytecode; driver-side
// memory behind a shader object is not visible here.
template <typename Compiled> class D3DShaderCache
{
public:
  static constexpr size_t kDefaultBudgetBytes = 8u << 20;

  explicit D3DShaderCache(size_t budgetBytes = kDefaultBudgetBytes)
      : m_budgetBytes(budgetBytes)
  {}

  // Copies the compile stored for `key` into `out` and marks it recently
  // used; false when there is none.
  bool lookup(const std::string& key, Compiled* out)
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_index.find(std::string_view(key));
    if (it == m_index.end())
    {
      return false;
    }
    m_entries.splice(m_entries.begin(), m_entries, it->second);
    *out = it->second->compiled;
    return true;
  }

  // Keeps `compiled` for `key`, charging `compiledBytes` plus the key against
  // the budget. An earlier store for the same key wins; both hold the same
  // bytecode, so nothing is lost.
  void store(
      const std::string& key,
      const Compiled& compiled,
      size_t compiledBytes)
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_index.find(std::string_view(key)) != m_index.end())
    {
      return;
    }
    m_entries.push_front(Entry{key, compiled, key.size() + compiledBytes});
    m_index.emplace(std::string_view(m_entries.front().key), m_entries.begin());
    m_totalBytes += m_entries.front().bytes;
    // The newest entry stays even when it alone exceeds the budget: it is
    // the one about to be used.
    while (m_totalBytes > m_budgetBytes && m_entries.size() > 1)
    {
      const Entry& oldest = m_entries.back();
      m_totalBytes -= oldest.bytes;
      m_index.erase(std::string_view(oldest.key));
      m_entries.pop_back();
    }
  }

  size_t size() const
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries.size();
  }

  size_t totalBytes() const
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalBytes;
  }

private:
  struct Entry
  {
    std::string key;
    Compiled compiled;
    size_t bytes;
  };

  mutable std::mutex m_mutex;
  const size_t m_budgetBytes;
  size_t m_totalBytes = 0;
  // Front is most recently used. List nodes do not move, so the index can
  // view each entry's key in place.
  std::list<Entry> m_entries;
  std::unordered_map<std::string_view, typename std::list<Entry>::iterator>
      m_index;
};

} // namespace rive::ore
