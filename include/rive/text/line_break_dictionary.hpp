/*
 * Copyright 2026 Rive
 */

#ifndef _RIVE_LINE_BREAK_DICTIONARY_HPP_
#define _RIVE_LINE_BREAK_DICTIONARY_HPP_

#include "rive/text/line_break.hpp"
#include <cstdint>
#ifdef WITH_RIVE_TEXT_DICT_COMPRESSED
#include <vector>
#endif

#if defined(WITH_RIVE_TEXT_DICT_THAI) || defined(WITH_RIVE_TEXT_DICT_LAO) ||   \
    defined(WITH_RIVE_TEXT_DICT_KHMER) || defined(WITH_RIVE_TEXT_DICT_BURMESE)
#define RIVE_LINE_BREAK_DICTIONARIES
#endif

namespace rive
{

// A word list for a script written without spaces between words (UAX #14
// class SA), stored as a minimized trie. Without one, LB1 turns the whole
// script into AL and an unspaced phrase becomes a single word. Each language
// is compiled in by its own WITH_RIVE_TEXT_DICT_<LANGUAGE> define, see
// dev/unicode/generate_line_break_dictionaries.py.
struct LineBreakDictionary
{
  // Inclusive {first, last} codepoint pairs the dictionary segments.
  Span<const uint32_t> ranges;
  // Sorted codepoints the words use; a codepoint's index is its symbol.
  Span<const uint32_t> alphabet;
  // Codepoints a word boundary may not precede or follow, so a syllable
  // stays whole when a word isn't in the list.
  Span<const uint32_t> noBreakBefore;
  Span<const uint32_t> noBreakAfter;
  // Repetition and abbreviation signs. They can't start a word either, they
  // belong to the word before them.
  Span<const uint32_t> suffixes;
  // Marks that make the consonant they directly follow end the previous
  // syllable (killers, asat, virama), so no boundary may precede that
  // consonant. Only codaSkip marks may sit between the two.
  Span<const uint32_t> codaMarks;
  Span<const uint32_t> codaSkip;
#ifdef WITH_RIVE_TEXT_DICT_COMPRESSED
  // The sorted words, front coded and deflated (raw RFC 1951), and their
  // inflated size. lineBreakDictionaryEdges() builds the trie from them on
  // first use.
  Span<const uint8_t> words;
  uint32_t wordsSize;
#else
  // The trie. Each state's edges are contiguous, sorted by symbol, and the
  // root's start at 0.
  Span<const uint32_t> edges;
#endif
  // Size and FNV-1a checksum of the trie's edges, so a rebuilt trie can be
  // checked against the one the generator made.
  uint32_t edgeCount;
  uint32_t edgeChecksum;

  static constexpr uint32_t symbolMask = 0x7F;
  // Set on the last edge of a state.
  static constexpr uint32_t lastEdge = 1 << 7;
  // Set when a word ends after this edge's symbol.
  static constexpr uint32_t wordEnd = 1 << 8;
  // The rest is the index of the target state's first edge, 0 when the
  // target has no edges.
  static constexpr uint32_t targetShift = 9;
};

#ifdef RIVE_LINE_BREAK_DICTIONARIES
// The compiled in dictionary that segments codepoint, if any.
const LineBreakDictionary* lineBreakDictionaryFor(uint32_t codepoint);

// The dictionary's trie edges. With WITH_RIVE_TEXT_DICT_COMPRESSED the first
// call builds them from the word list, later calls share that copy.
Span<const uint32_t> lineBreakDictionaryEdges(
    const LineBreakDictionary& dictionary);

#ifdef WITH_RIVE_TEXT_DICT_COMPRESSED
// Inflates the dictionary's word list and builds its trie, the same edges
// the generator would have compiled in. Empty if the data is corrupt.
std::vector<uint32_t> buildLineBreakDictionaryEdges(
    const LineBreakDictionary& dictionary);
#endif

// Adds break opportunities between dictionary words inside every run of a
// dictionary's script. Only turns none into allowed, so it runs after the
// UAX #14 rules have filled out.
void applyLineBreakDictionaries(Span<const uint32_t> text, Span<LineBreak> out);
#endif

} // namespace rive
#endif
