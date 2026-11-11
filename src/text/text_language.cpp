#include "rive/text/text_language.hpp"
#include "rive/core/binary_reader.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/data_resolver.hpp"
#include "rive/text_engine.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <unordered_map>

using namespace rive;

namespace
{
std::mutex g_mutex;
// Slot 0 stays empty. A slot is written once, before g_count publishes it,
// so tag() reads it without the lock.
std::array<std::string, TextLanguages::maxCount + 1> g_tags;
std::atomic<size_t> g_count{1};
std::unordered_map<std::string, TextLanguageId> g_slots;
bool g_loggedFull = false;

std::vector<std::string> g_systemLanguages;
std::atomic<uint32_t> g_generation{0};

const std::string g_empty;
} // namespace

TextLanguageId TextLanguages::intern(const std::string& tag)
{
  if (tag.empty())
  {
    return none;
  }
  std::lock_guard<std::mutex> lock(g_mutex);
  auto itr = g_slots.find(tag);
  if (itr != g_slots.end())
  {
    return itr->second;
  }
  size_t count = g_count.load(std::memory_order_relaxed);
  if (count > maxCount)
  {
    if (!g_loggedFull)
    {
      g_loggedFull = true;
      fprintf(
          stderr,
          "TextLanguages: more than %zu language tags, \"%s\" and any "
          "later ones are ignored.\n",
          maxCount,
          tag.c_str());
    }
    return none;
  }
  auto id = (TextLanguageId)count;
  g_tags[count] = tag;
  g_slots[tag] = id;
  g_count.store(count + 1, std::memory_order_release);
  return id;
}

const std::string& TextLanguages::tag(TextLanguageId id)
{
  if (id == none || id >= g_count.load(std::memory_order_acquire))
  {
    return g_empty;
  }
  return g_tags[id];
}

void TextLanguages::setSystemLanguages(Span<const std::string> languages)
{
  std::vector<std::string> unique;
  unique.reserve(languages.size());
  for (const std::string& language : languages)
  {
    if (!language.empty() &&
        std::find(unique.begin(), unique.end(), language) == unique.end())
    {
      unique.push_back(language);
    }
  }
  std::lock_guard<std::mutex> lock(g_mutex);
  if (unique == g_systemLanguages)
  {
    return;
  }
  g_systemLanguages = std::move(unique);
#ifdef WITH_RIVE_TEXT
  // Text that inherits shapes and falls back differently now, so no shape
  // made before may be reused. Moved before the generation is: a text that
  // sees the new generation then also sees its cached shapes are stale,
  // rather than reshaping into one of them.
  Font::fallbackFontsChanged();
#endif
  g_generation.fetch_add(1, std::memory_order_acq_rel);
}

std::vector<std::string> TextLanguages::systemLanguages()
{
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_systemLanguages;
}

uint32_t TextLanguages::generation()
{
  return g_generation.load(std::memory_order_acquire);
}

std::vector<std::string> TextLanguages::fallbackLanguages(TextLanguageId id)
{
  const std::string& own = tag(id);
  std::lock_guard<std::mutex> lock(g_mutex);
  std::vector<std::string> languages;
  languages.reserve(g_systemLanguages.size() + 1);
  if (!own.empty())
  {
    languages.push_back(own);
  }
  for (const std::string& language : g_systemLanguages)
  {
    if (language != own)
    {
      languages.push_back(language);
    }
  }
  return languages;
}

std::string TextLanguages::shapingLanguage(TextLanguageId id)
{
  const std::string& own = tag(id);
  if (!own.empty())
  {
    return own;
  }
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_systemLanguages.empty() ? std::string() : g_systemLanguages.front();
}

std::string TextLanguages::readManifestTag(BinaryReader& reader)
{
  uint32_t index = CoreUintType::deserialize(reader);
  DataResolver* resolver = reader.dataResolver();
  if (resolver == nullptr || reader.hasError())
  {
    return std::string();
  }
  return resolver->resolveName((int)index);
}

#ifdef TESTING
void TextLanguages::resetForTesting()
{
  std::lock_guard<std::mutex> lock(g_mutex);
  for (size_t i = 1; i < g_count.load(std::memory_order_relaxed); i++)
  {
    g_tags[i].clear();
  }
  g_slots.clear();
  g_count.store(1, std::memory_order_release);
  g_loggedFull = false;
  if (!g_systemLanguages.empty())
  {
    g_systemLanguages.clear();
    g_generation.fetch_add(1, std::memory_order_acq_rel);
  }
}
#endif
