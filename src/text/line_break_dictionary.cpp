/*
 * Copyright 2026 Rive
 */

#include "rive/text/line_break_dictionary.hpp"

#ifdef RIVE_LINE_BREAK_DICTIONARIES
#include <algorithm>
#include <vector>
#ifdef WITH_RIVE_TEXT_DICT_COMPRESSED
#include <mutex>
#endif

using namespace rive;

namespace rive
{
#ifdef WITH_RIVE_TEXT_DICT_THAI
extern const LineBreakDictionary kLineBreakDictionaryThai;
#endif
#ifdef WITH_RIVE_TEXT_DICT_LAO
extern const LineBreakDictionary kLineBreakDictionaryLao;
#endif
#ifdef WITH_RIVE_TEXT_DICT_KHMER
extern const LineBreakDictionary kLineBreakDictionaryKhmer;
#endif
#ifdef WITH_RIVE_TEXT_DICT_BURMESE
extern const LineBreakDictionary kLineBreakDictionaryBurmese;
#endif
} // namespace rive

namespace
{
const LineBreakDictionary* const kDictionaries[] = {
#ifdef WITH_RIVE_TEXT_DICT_THAI
    &kLineBreakDictionaryThai,
#endif
#ifdef WITH_RIVE_TEXT_DICT_LAO
    &kLineBreakDictionaryLao,
#endif
#ifdef WITH_RIVE_TEXT_DICT_KHMER
    &kLineBreakDictionaryKhmer,
#endif
#ifdef WITH_RIVE_TEXT_DICT_BURMESE
    &kLineBreakDictionaryBurmese,
#endif
};

bool inRanges(const LineBreakDictionary& dictionary, uint32_t codepoint)
{
  Span<const uint32_t> ranges = dictionary.ranges;
  if (codepoint < ranges.front() || codepoint > ranges.back())
  {
    return false;
  }
  for (size_t i = 0; i + 1 < ranges.size(); i += 2)
  {
    if (codepoint >= ranges[i] && codepoint <= ranges[i + 1])
    {
      return true;
    }
  }
  return false;
}

bool isJoiner(uint32_t codepoint)
{
  return codepoint == 0x200C || codepoint == 0x200D;
}

bool contains(Span<const uint32_t> sorted, uint32_t codepoint)
{
  return std::binary_search(sorted.begin(), sorted.end(), codepoint);
}

// Combining marks and joiners, which stay with the codepoint before them.
bool isMark(uint32_t codepoint)
{
  return isJoiner(codepoint) ||
         lineBreakProps(codepoint).cls == LineBreakClass::CM;
}

// Whether run[i] is a consonant directly followed by a coda mark, which
// makes it end the previous syllable.
bool isFinalConsonant(
    const LineBreakDictionary& dictionary,
    const uint32_t* run,
    size_t count,
    size_t i)
{
  size_t j = i + 1;
  while (j < count && contains(dictionary.codaSkip, run[j]))
  {
    j++;
  }
  return j < count && contains(dictionary.codaMarks, run[j]);
}

// Whether a word boundary may sit before run[i], 0 < i < count, without
// splitting a syllable.
bool canBreak(
    const LineBreakDictionary& dictionary,
    const uint32_t* run,
    size_t count,
    size_t i)
{
  uint32_t prev = run[i - 1];
  uint32_t next = run[i];
  return !isJoiner(prev) && !isMark(next) &&
         !contains(dictionary.noBreakBefore, next) &&
         !contains(dictionary.suffixes, next) &&
         !contains(dictionary.noBreakAfter, prev) &&
         !isFinalConsonant(dictionary, run, count, i);
}

constexpr uint32_t kUnreached = UINT32_MAX;

// One per boundary of a run, the best segmentation of the run up to it.
struct Cell
{
  int32_t symbol;    // of the codepoint after the boundary, -1 if none
  bool canBreak;     // a segment may start or end here
  bool word;         // the segment ending here is a dictionary word
  uint32_t unknown;  // codepoints no word covers, kUnreached until reached
  uint32_t segments; // segments so far
  uint32_t from;     // where the segment ending here starts
};

// Fewest codepoints outside words wins, then fewest segments, so longer
// words beat splitting them into shorter ones.
void relax(Cell* cells, size_t from, size_t to, uint32_t unknown, bool word)
{
  uint32_t total = cells[from].unknown + unknown;
  uint32_t segments = cells[from].segments + 1;
  Cell& cell = cells[to];
  if (total < cell.unknown ||
      (total == cell.unknown && segments < cell.segments))
  {
    cell.unknown = total;
    cell.segments = segments;
    cell.from = (uint32_t)from;
    cell.word = word;
  }
}

uint32_t edgeSymbol(uint32_t edge)
{
  return edge & LineBreakDictionary::symbolMask;
}

// Returns the edge leaving the state at first for symbol, or -1. The root
// has an edge for nearly every symbol, so it gets a binary search over its
// rootCount edges.
int64_t findEdge(
    Span<const uint32_t> edges,
    uint32_t first,
    uint32_t symbol,
    uint32_t rootCount)
{
  if (first == 0)
  {
    const uint32_t* end = edges.data() + rootCount;
    const uint32_t* found = std::lower_bound(
        edges.data(),
        end,
        symbol,
        [](uint32_t edge, uint32_t symbol) {
          return edgeSymbol(edge) < symbol;
        });
    return found != end && edgeSymbol(*found) == symbol ? found - edges.data()
                                                        : -1;
  }
  for (uint32_t i = first; i < edges.size(); i++)
  {
    uint32_t edge = edges[i];
    if (edgeSymbol(edge) == symbol)
    {
      return i;
    }
    if (edgeSymbol(edge) > symbol ||
        (edge & LineBreakDictionary::lastEdge) != 0)
    {
      break;
    }
  }
  return -1;
}

// Where a word that ends before run[end] really ends: past the suffixes and
// killed final consonants that belong to it, or 0 when it would end mid
// syllable, which makes the match spurious.
size_t extendWordEnd(
    const LineBreakDictionary& dictionary,
    const uint32_t* run,
    size_t count,
    const Cell* cells,
    size_t end)
{
  while (!cells[end].canBreak)
  {
    if (contains(dictionary.suffixes, run[end]))
    {
      end++;
    }
    else if (!isMark(run[end]) && isFinalConsonant(dictionary, run, count, end))
    {
      // The consonant and every mark on it.
      end++;
      while (end < count && isMark(run[end]))
      {
        end++;
      }
    }
    else
    {
      return 0;
    }
  }
  return end;
}

// Segments run[0, count) into dictionary words, keeping any stretch no word
// covers whole, and allows a break at the start of every segment.
void segmentRun(
    const LineBreakDictionary& dictionary,
    Span<const uint32_t> edges,
    const uint32_t* run,
    size_t count,
    LineBreak* out)
{
  // Cells live on the stack for typical runs.
  constexpr size_t kInlineCells = 128;
  Cell inlineCells[kInlineCells + 1];
  std::vector<Cell> heapCells;
  Cell* cells = inlineCells;
  if (count > kInlineCells)
  {
    heapCells.resize(count + 1);
    cells = heapCells.data();
  }
  Span<const uint32_t> alphabet = dictionary.alphabet;
  for (size_t i = 0; i <= count; i++)
  {
    Cell& cell = cells[i];
    cell.symbol = -1;
    if (i < count)
    {
      auto found = std::lower_bound(alphabet.begin(), alphabet.end(), run[i]);
      if (found != alphabet.end() && *found == run[i])
      {
        cell.symbol = (int32_t)(found - alphabet.begin());
      }
    }
    cell.canBreak = i == 0 || i == count || canBreak(dictionary, run, count, i);
    cell.word = false;
    cell.unknown = kUnreached;
    cell.segments = 0;
    cell.from = 0;
  }
  cells[0].unknown = 0;

  uint32_t rootCount = 1;
  while ((edges[rootCount - 1] & LineBreakDictionary::lastEdge) == 0)
  {
    rootCount++;
  }
  for (size_t i = 0; i < count; i++)
  {
    if (cells[i].unknown == kUnreached)
    {
      continue;
    }
    // Every dictionary word starting here.
    uint32_t state = 0;
    for (size_t j = i; j < count && cells[j].symbol >= 0; j++)
    {
      int64_t edge = findEdge(
          edges,
          state,
          (uint32_t)cells[j].symbol,
          rootCount);
      if (edge < 0)
      {
        break;
      }
      uint32_t value = edges[(size_t)edge];
      if ((value & LineBreakDictionary::wordEnd) != 0)
      {
        size_t end = extendWordEnd(dictionary, run, count, cells, j + 1);
        if (end != 0)
        {
          // What the word took along counts as outside any word, so an
          // exact word still wins.
          relax(cells, i, end, (uint32_t)(end - j - 1), true);
        }
      }
      state = value >> LineBreakDictionary::targetShift;
      if (state == 0)
      {
        break;
      }
    }
    // Or the syllable here, outside any word.
    size_t next = i + 1;
    while (!cells[next].canBreak)
    {
      next++;
    }
    relax(cells, i, next, (uint32_t)(next - i), false);
  }

  // Walk the segments back from the end. Neighbouring stretches outside any
  // word stay joined.
  for (size_t end = count; end > 0;)
  {
    size_t start = cells[end].from;
    if (start > 0 && (cells[end].word || cells[start].word) &&
        out[start] == LineBreak::none)
    {
      out[start] = LineBreak::allowed;
    }
    end = start;
  }
}
} // namespace

#ifdef WITH_RIVE_TEXT_DICT_COMPRESSED
Span<const uint32_t> rive::lineBreakDictionaryEdges(
    const LineBreakDictionary& dictionary)
{
  // Built at most once per dictionary, and only for scripts the text uses.
  struct Built
  {
    std::once_flag once;
    std::vector<uint32_t> edges;
  };
  constexpr size_t kCount = sizeof(kDictionaries) / sizeof(kDictionaries[0]);
  static Built built[kCount];
  for (size_t i = 0; i < kCount; i++)
  {
    if (kDictionaries[i] == &dictionary)
    {
      std::call_once(built[i].once, [&] {
        built[i].edges = buildLineBreakDictionaryEdges(dictionary);
      });
      return built[i].edges;
    }
  }
  return {};
}
#else
Span<const uint32_t> rive::lineBreakDictionaryEdges(
    const LineBreakDictionary& dictionary)
{
  return dictionary.edges;
}
#endif

const LineBreakDictionary* rive::lineBreakDictionaryFor(uint32_t codepoint)
{
  for (const LineBreakDictionary* dictionary : kDictionaries)
  {
    if (inRanges(*dictionary, codepoint))
    {
      return dictionary;
    }
  }
  return nullptr;
}

void rive::applyLineBreakDictionaries(
    Span<const uint32_t> text,
    Span<LineBreak> out)
{
  // Codepoints outside every dictionary's span skip the per dictionary
  // checks, which keeps Latin text cheap.
  uint32_t first = UINT32_MAX;
  uint32_t last = 0;
  for (const LineBreakDictionary* dictionary : kDictionaries)
  {
    first = std::min(first, dictionary->ranges.front());
    last = std::max(last, dictionary->ranges.back());
  }
  size_t count = text.size();
  for (size_t i = 0; i < count;)
  {
    if (text[i] < first || text[i] > last)
    {
      i++;
      continue;
    }
    // A run never starts on a mark: one there belongs to whatever precedes
    // it, like an opening bracket, and breaking after it would strand them.
    const LineBreakDictionary* dictionary = lineBreakDictionaryFor(text[i]);
    if (dictionary == nullptr || isMark(text[i]))
    {
      i++;
      continue;
    }
    size_t end = i + 1;
    while (end < count &&
           (inRanges(*dictionary, text[end]) || isJoiner(text[end])))
    {
      end++;
    }
    // A single codepoint has nothing to split, so it never builds a
    // compressed dictionary.
    if (end - i > 1)
    {
      Span<const uint32_t> edges = lineBreakDictionaryEdges(*dictionary);
      if (!edges.empty())
      {
        segmentRun(
            *dictionary,
            edges,
            text.data() + i,
            end - i,
            out.data() + i);
      }
    }
    i = end;
  }
}
#endif
