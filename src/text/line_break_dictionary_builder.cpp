/*
 * Copyright 2026 Rive
 */

#include "rive/text/line_break_dictionary.hpp"

#if defined(RIVE_LINE_BREAK_DICTIONARIES) &&                                   \
    defined(WITH_RIVE_TEXT_DICT_COMPRESSED)
#include <cstring>
#include <unordered_set>

using namespace rive;

namespace
{
// A raw deflate (RFC 1951) decoder. Small rather than fast, it runs once per
// dictionary.
class Inflater
{
public:
  Inflater(Span<const uint8_t> in, uint8_t* out, size_t outSize)
      : m_in(in)
      , m_out(out)
      , m_outSize(outSize)
  {}

  // Whether the stream decoded to exactly outSize bytes.
  bool run()
  {
    bool last = false;
    while (!last && !m_failed)
    {
      last = bits(1) != 0;
      bool ok = false;
      switch (bits(2))
      {
        case 0:
          ok = stored();
          break;
        case 1:
          ok = fixed();
          break;
        case 2:
          ok = dynamic();
          break;
      }
      if (!ok)
      {
        return false;
      }
    }
    return !m_failed && m_outPos == m_outSize;
  }

private:
  static constexpr int kMaxBits = 15;

  // Canonical code: how many codes have each length, and the symbols
  // ordered by code.
  struct Huffman
  {
    uint16_t count[kMaxBits + 1];
    uint16_t symbol[288];
  };

  // Reads n bits, least significant first. Past the end of the input it
  // returns zeros and flags the failure.
  uint32_t bits(int n)
  {
    while (m_bitCount < n)
    {
      if (m_inPos == m_in.size())
      {
        m_failed = true;
        return 0;
      }
      m_bitBuffer |= (uint32_t)m_in[m_inPos++] << m_bitCount;
      m_bitCount += 8;
    }
    uint32_t value = m_bitBuffer & ((1u << n) - 1);
    m_bitBuffer >>= n;
    m_bitCount -= n;
    return value;
  }

  bool stored()
  {
    // Drop the rest of the current byte, then LEN and its complement.
    m_bitBuffer = 0;
    m_bitCount = 0;
    if (m_in.size() - m_inPos < 4)
    {
      return false;
    }
    uint32_t length = m_in[m_inPos] | (m_in[m_inPos + 1] << 8);
    uint32_t complement = m_in[m_inPos + 2] | (m_in[m_inPos + 3] << 8);
    m_inPos += 4;
    if (length != (~complement & 0xFFFF) || m_in.size() - m_inPos < length ||
        m_outSize - m_outPos < length)
    {
      return false;
    }
    memcpy(m_out + m_outPos, m_in.data() + m_inPos, length);
    m_inPos += length;
    m_outPos += length;
    return true;
  }

  // Fails on an over-subscribed set of lengths. Incomplete codes are fine,
  // deflate uses them for single distance codes.
  static bool construct(Huffman& huffman, const uint8_t* lengths, int count)
  {
    memset(huffman.count, 0, sizeof(huffman.count));
    for (int i = 0; i < count; i++)
    {
      huffman.count[lengths[i]]++;
    }
    int left = 1;
    for (int length = 1; length <= kMaxBits; length++)
    {
      left = (left << 1) - huffman.count[length];
      if (left < 0)
      {
        return false;
      }
    }
    uint16_t offsets[kMaxBits + 1];
    offsets[1] = 0;
    for (int length = 1; length < kMaxBits; length++)
    {
      offsets[length + 1] = offsets[length] + huffman.count[length];
    }
    for (int i = 0; i < count; i++)
    {
      if (lengths[i] != 0)
      {
        huffman.symbol[offsets[lengths[i]]++] = (uint16_t)i;
      }
    }
    return true;
  }

  // Decodes one symbol a bit at a time, or returns -1.
  int decode(const Huffman& huffman)
  {
    int code = 0;
    int first = 0;
    int index = 0;
    for (int length = 1; length <= kMaxBits; length++)
    {
      code |= (int)bits(1);
      int count = huffman.count[length];
      if (code - count < first)
      {
        return huffman.symbol[index + (code - first)];
      }
      index += count;
      first = (first + count) << 1;
      code <<= 1;
    }
    return -1;
  }

  bool codes(const Huffman& lengthCode, const Huffman& distanceCode)
  {
    static const uint16_t kLengthBase[29] = {
        3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
        31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const uint8_t kLengthExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
                                             1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
                                             4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const uint16_t kDistanceBase[30] = {
        1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
        33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
        1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const uint8_t kDistanceExtra[30] = {
        0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
        6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    while (!m_failed)
    {
      int symbol = decode(lengthCode);
      if (symbol < 0)
      {
        return false;
      }
      if (symbol < 256)
      {
        if (m_outPos == m_outSize)
        {
          return false;
        }
        m_out[m_outPos++] = (uint8_t)symbol;
        continue;
      }
      if (symbol == 256)
      {
        return !m_failed;
      }
      symbol -= 257;
      if (symbol >= 29)
      {
        return false;
      }
      size_t length = kLengthBase[symbol] + bits(kLengthExtra[symbol]);
      int distanceSymbol = decode(distanceCode);
      if (distanceSymbol < 0 || distanceSymbol >= 30)
      {
        return false;
      }
      size_t distance = kDistanceBase[distanceSymbol] +
                        bits(kDistanceExtra[distanceSymbol]);
      if (distance > m_outPos || length > m_outSize - m_outPos)
      {
        return false;
      }
      // Byte by byte, the copy may overlap what it writes.
      for (size_t i = 0; i < length; i++, m_outPos++)
      {
        m_out[m_outPos] = m_out[m_outPos - distance];
      }
    }
    return false;
  }

  bool fixed()
  {
    uint8_t lengths[288 + 30];
    for (int i = 0; i < 288; i++)
    {
      lengths[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
    }
    for (int i = 0; i < 30; i++)
    {
      lengths[288 + i] = 5;
    }
    Huffman lengthCode;
    Huffman distanceCode;
    construct(lengthCode, lengths, 288);
    construct(distanceCode, lengths + 288, 30);
    return codes(lengthCode, distanceCode);
  }

  bool dynamic()
  {
    static const uint8_t kOrder[19] =
        {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    int lengthCount = (int)bits(5) + 257;
    int distanceCount = (int)bits(5) + 1;
    int codeCount = (int)bits(4) + 4;
    if (lengthCount > 286 || distanceCount > 30)
    {
      return false;
    }
    uint8_t codeLengths[19] = {};
    for (int i = 0; i < codeCount; i++)
    {
      codeLengths[kOrder[i]] = (uint8_t)bits(3);
    }
    Huffman codeCode;
    if (!construct(codeCode, codeLengths, 19))
    {
      return false;
    }
    uint8_t lengths[286 + 30] = {};
    int total = lengthCount + distanceCount;
    for (int index = 0; index < total;)
    {
      int symbol = decode(codeCode);
      if (symbol < 0 || m_failed)
      {
        return false;
      }
      if (symbol < 16)
      {
        lengths[index++] = (uint8_t)symbol;
        continue;
      }
      uint8_t repeat = 0;
      int times;
      if (symbol == 16)
      {
        if (index == 0)
        {
          return false;
        }
        repeat = lengths[index - 1];
        times = 3 + (int)bits(2);
      }
      else if (symbol == 17)
      {
        times = 3 + (int)bits(3);
      }
      else
      {
        times = 11 + (int)bits(7);
      }
      if (index + times > total)
      {
        return false;
      }
      while (times-- > 0)
      {
        lengths[index++] = repeat;
      }
    }
    // The end of block code has to exist.
    if (lengths[256] == 0)
    {
      return false;
    }
    Huffman lengthCode;
    Huffman distanceCode;
    if (!construct(lengthCode, lengths, lengthCount) ||
        !construct(distanceCode, lengths + lengthCount, distanceCount))
    {
      return false;
    }
    return codes(lengthCode, distanceCode);
  }

  Span<const uint8_t> m_in;
  size_t m_inPos = 0;
  uint32_t m_bitBuffer = 0;
  int m_bitCount = 0;
  uint8_t* m_out;
  size_t m_outSize;
  size_t m_outPos = 0;
  bool m_failed = false;
};

// Builds the minimized trie of words added in sorted order, following Daciuk
// et al., "Incremental construction of minimal acyclic finite-state
// automata". A word's end is marked on the edge that completes it, so a
// state is nothing but its outgoing edges.
class TrieBuilder
{
public:
  TrieBuilder() : m_registry(1024, Hash{&m_states}, Equal{&m_states})
  {
    m_states.emplace_back(); // the root
    m_path.push_back(0);
  }

  // Adds word[0, length), which shares its first shared symbols with the
  // previous word.
  void add(const uint8_t* word, size_t length, size_t shared)
  {
    minimize(shared);
    m_path.resize(shared + 1);
    for (size_t i = shared; i < length; i++)
    {
      uint32_t state = newState();
      m_states[m_path.back()].push_back({word[i], i + 1 == length, state});
      m_path.push_back(state);
    }
  }

  // Lays the trie out breadth first from the root, as the generator does,
  // so the edges come out identical to the ones it would compile in.
  std::vector<uint32_t> finish()
  {
    minimize(0);
    std::vector<uint32_t> firstEdge(m_states.size(), 0);
    std::vector<bool> seen(m_states.size(), false);
    std::vector<uint32_t> order = {0};
    seen[0] = true;
    uint32_t edgeCount = 0;
    for (size_t i = 0; i < order.size(); i++)
    {
      const std::vector<Edge>& edges = m_states[order[i]];
      if (edges.empty())
      {
        continue;
      }
      firstEdge[order[i]] = edgeCount;
      edgeCount += (uint32_t)edges.size();
      for (const Edge& edge : edges)
      {
        if (!seen[edge.target])
        {
          seen[edge.target] = true;
          order.push_back(edge.target);
        }
      }
    }
    std::vector<uint32_t> result;
    result.reserve(edgeCount);
    for (uint32_t state : order)
    {
      const std::vector<Edge>& edges = m_states[state];
      for (size_t i = 0; i < edges.size(); i++)
      {
        const Edge& edge = edges[i];
        result.push_back(
            edge.symbol |
            (i + 1 == edges.size() ? LineBreakDictionary::lastEdge : 0) |
            (edge.wordEnd ? LineBreakDictionary::wordEnd : 0) |
            (firstEdge[edge.target] << LineBreakDictionary::targetShift));
      }
    }
    return result;
  }

private:
  struct Edge
  {
    uint8_t symbol;
    bool wordEnd;
    uint32_t target;

    bool operator==(const Edge& other) const
    {
      return symbol == other.symbol && wordEnd == other.wordEnd &&
             target == other.target;
    }
  };

  using States = std::vector<std::vector<Edge>>;

  struct Hash
  {
    const States* states;
    size_t operator()(uint32_t state) const
    {
      uint64_t hash = 14695981039346656037ull;
      for (const Edge& edge : (*states)[state])
      {
        uint64_t value = edge.symbol | (edge.wordEnd ? 1u << 8 : 0) |
                         ((uint64_t)edge.target << 9);
        hash = (hash ^ value) * 1099511628211ull;
      }
      return (size_t)(hash ^ (hash >> 32));
    }
  };

  struct Equal
  {
    const States* states;
    bool operator()(uint32_t a, uint32_t b) const
    {
      return (*states)[a] == (*states)[b];
    }
  };

  uint32_t newState()
  {
    if (!m_free.empty())
    {
      uint32_t state = m_free.back();
      m_free.pop_back();
      m_states[state].clear();
      return state;
    }
    m_states.emplace_back();
    return (uint32_t)(m_states.size() - 1);
  }

  // Replaces every state on the previous word's path deeper than depth with
  // an equivalent registered one, or registers it, deepest first so its
  // children are already final.
  void minimize(size_t depth)
  {
    for (size_t d = m_path.size() - 1; d > depth; d--)
    {
      uint32_t state = m_path[d];
      auto found = m_registry.find(state);
      if (found != m_registry.end())
      {
        m_states[m_path[d - 1]].back().target = *found;
        m_free.push_back(state);
      }
      else
      {
        m_registry.insert(state);
      }
    }
  }

  States m_states;
  std::vector<uint32_t> m_free;
  // States along the last word added, the root first.
  std::vector<uint32_t> m_path;
  std::unordered_set<uint32_t, Hash, Equal> m_registry;
};
} // namespace

std::vector<uint32_t> rive::buildLineBreakDictionaryEdges(
    const LineBreakDictionary& dictionary)
{
  std::vector<uint8_t> coded(dictionary.wordsSize);
  if (!Inflater(dictionary.words, coded.data(), coded.size()).run())
  {
    return {};
  }
  // Each word is the length it shares with the previous one, its remaining
  // symbols plus one, and a 0.
  TrieBuilder builder;
  uint8_t word[256];
  size_t length = 0;
  for (size_t i = 0; i < coded.size();)
  {
    size_t shared = coded[i++];
    if (shared > length)
    {
      return {};
    }
    length = shared;
    while (i < coded.size() && coded[i] != 0)
    {
      if (length == sizeof(word) || coded[i] > dictionary.alphabet.size())
      {
        return {};
      }
      word[length++] = coded[i++] - 1;
    }
    if (i == coded.size() || length == shared)
    {
      return {};
    }
    i++;
    builder.add(word, length, shared);
  }
  std::vector<uint32_t> edges = builder.finish();
  // Anything but the generator's trie would segment wrongly, so a mismatch
  // leaves the script unsegmented instead.
  uint32_t checksum = 2166136261u;
  for (uint32_t edge : edges)
  {
    checksum = (checksum ^ edge) * 16777619u;
  }
  if (edges.size() != dictionary.edgeCount ||
      checksum != dictionary.edgeChecksum)
  {
    return {};
  }
  return edges;
}
#endif
