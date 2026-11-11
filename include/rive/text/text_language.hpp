#ifndef _RIVE_TEXT_LANGUAGE_HPP_
#define _RIVE_TEXT_LANGUAGE_HPP_

#include "rive/span.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace rive
{
class BinaryReader;

/// A slot in the process-wide table of language strings (see
/// TextLanguages). 0 is no language: the text inherits one.
using TextLanguageId = uint8_t;

/// The languages texts are shaped and fall back with.
///
/// A text names one language, like HTML's lang, and inherits it when empty:
/// a run from its Text, a Text from the host's system languages. It is
/// usually a BCP 47 tag, but any string is kept as written and reaches the
/// shaper and the host's fallback requests unchanged, so a host can use
/// formats of its own. Languages are interned into a table that starts empty
/// and only holds the ones this process has met, so TextRun can carry one in
/// a byte. The table holds at most 255; past that a new one logs once and
/// behaves as no language.
///
/// The system languages are the host's ordered preference list, the
/// equivalent of navigator.languages. A host sets them once it knows them and
/// again whenever they change.
class TextLanguages
{
public:
  static constexpr TextLanguageId none = 0;
  static constexpr size_t maxCount = 255;

  /// The slot holding [tag], exactly as written, added if new. none when
  /// [tag] is empty or the table is full.
  static TextLanguageId intern(const std::string& tag);

  /// The language in [id], empty for none or a slot never handed out.
  static const std::string& tag(TextLanguageId id);

  /// The host's preferred languages, most preferred first, kept as written.
  /// Empty ones and repeats are dropped.
  static void setSystemLanguages(Span<const std::string> languages);
  static std::vector<std::string> systemLanguages();

  /// Moves whenever setSystemLanguages() changes the list.
  static uint32_t generation();

  /// The tags a fallback request lists for text in [id]: its tag first, then
  /// the system languages, without repeats.
  static std::vector<std::string> fallbackLanguages(TextLanguageId id);

  /// The tag text in [id] shapes with: its own, else the most preferred
  /// system language. Empty when there's neither, which leaves the shaper
  /// at its default.
  static std::string shapingLanguage(TextLanguageId id);

  /// Reads a languageId property, which a file writes in place of language:
  /// the tag at that index of the manifest's names. Empty, so inherit, when
  /// the reader has no manifest or the index names nothing.
  static std::string readManifestTag(BinaryReader& reader);

#ifdef TESTING
  /// Empties the table and the system languages.
  static void resetForTesting();
#endif
};
} // namespace rive

#endif
