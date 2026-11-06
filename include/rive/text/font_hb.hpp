#ifndef _RIVE_FONT_HB_HPP_
#define _RIVE_FONT_HB_HPP_

#include "rive/factory.hpp"
#include "rive/text_engine.hpp"

#include <atomic>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>

// Platforms where HBFont::DecodeFile can map a file. Declared here so callers
// and tests branch on the same condition the implementation uses.
#if defined(__APPLE__) || defined(__linux__) || defined(__ANDROID__)
#define RIVE_HB_FILE_MAPPING 1
#endif

struct hb_font_t;
struct hb_draw_funcs_t;
struct hb_paint_funcs_t;
struct hb_feature_t;
using hb_color_t = uint32_t;

class HBFont : public rive::Font
{
public:
  /** A mapped face for nominal coverage queries, without Rive font
   * initialization. */
  class FileProbe
  {
  public:
    FileProbe(const FileProbe&) = delete;
    FileProbe& operator=(const FileProbe&) = delete;
    /** Releases the probe's font and its reference to the mapped file. */
    ~FileProbe();
    /**
     * Checks nominal coverage without shaping, metrics, or drawing setup.
     * @param codepoint Unicode character to look up.
     * @return Whether the face contains the character, or false after
     * promotion.
     */
    bool hasGlyph(rive::Unichar codepoint) const;
    /**
     * Transfers the existing HarfBuzz font and mapping into a Rive font.
     * @return The initialized font, or nullptr if already promoted.
     */
    rive::rcp<rive::Font> makeFont();

  private:
    friend class HBFont;
    /** Takes ownership of a HarfBuzz font and enables nominal glyph lookup.
     */
    explicit FileProbe(hb_font_t* font);
    hb_font_t* m_font;
  };

  /**
   * Maps a file for coverage queries without initializing Rive font metrics
   * or drawing. The trusted, stable file restrictions documented on
   * DecodeFile also apply here.
   * @param path Stable local font file to map.
   * @param faceIndex Face within a font collection.
   * @return An owning probe, or nullptr on detectable failure or unsupported
   * platforms.
   */
  static std::unique_ptr<FileProbe> ProbeFile(
      const char* path,
      unsigned faceIndex = 0);

  // We assume ownership of font!
  HBFont(hb_font_t* font);
  ~HBFont() override;

  Axis getAxis(uint16_t index) const override;
  uint16_t getAxisCount() const override;
  float getAxisValue(uint32_t axisTag) const override;
  uint32_t getFeatureValue(uint32_t featureTag) const override;
  uint16_t getWeight() const override;
  bool isItalic() const override;

  rive::RawPath getPath(rive::GlyphID) const override;
  const rive::RawPath& glyphPath(rive::GlyphID) const override;
  const rive::RawPath* cachedGlyphPath(rive::GlyphID) const override;
  size_t glyphPathCacheRoom() const override;
  bool hasColorGlyphs() const override;
  bool isColorGlyph(rive::GlyphID) const override;
  size_t getColorLayers(
      rive::GlyphID,
      std::vector<ColorGlyphLayer>& out,
      rive::ColorInt foreground = 0xFF000000) const override;
  rive::SimpleArray<rive::Paragraph> onShapeText(
      rive::Span<const rive::Unichar>,
      rive::Span<const rive::TextRun>,
      int textDirectionFlag) const override;
  rive::SimpleArray<uint32_t> features() const override;
  rive::rcp<Font> withOptions(
      rive::Span<const Coord> variableAxes,
      rive::Span<const Feature> features) const override;
  rive::rcp<Font> sharedWithOptions(
      rive::Span<const Coord> variableAxes,
      rive::Span<const Feature> features) const override;

  // The font for the runs this font shapes as a fallback on the calling
  // thread. gFallbackProc hands every thread the same fonts, and a font's
  // caches aren't locked (see Font::sharedWithOptions), so only the first
  // thread to ask gets this font, and a host that shapes on one thread
  // draws the fonts its proc returns. Any other thread gets a copy of its
  // own, which its runs share while one holds it. A copy keeps this font's
  // face, coordinates, features and metrics, and draws its outlines through
  // this font, as a subclass may draw them its own way.
  rive::rcp<rive::Font> forThisThread() const;

  bool hasGlyph(const rive::Unichar) const override;
  std::vector<rive::Unichar> codepoints() const override;

  static rive::rcp<rive::Font> Decode(rive::Span<const uint8_t>);
  /// Decodes [bytes] in place, keeping them for the font's lifetime instead
  /// of copying them.
  static rive::rcp<rive::Font> Decode(std::vector<uint8_t>&& bytes);

  /// Decodes the font at [path] by mapping the file rather than copying it.
  /// [faceIndex] picks a face within a font collection (.ttc).
  ///
  /// Only for trusted, stable local files -- system fonts are the intended
  /// use. The mapping stays live for the font's lifetime, and a null return
  /// covers ONLY failures detectable during this call (unsupported platform,
  /// missing/unreadable/empty file, oversized file, a blob harfbuzz refuses).
  /// It is not a guarantee about later access: if the file is truncated or
  /// its backing storage disappears, faulting a page in raises SIGBUS and
  /// terminates the process. Do not point this at user-supplied paths,
  /// removable media or network volumes -- use Decode with your own bytes.
  ///
  /// Returns null where RIVE_HB_FILE_MAPPING is undefined, so callers must
  /// always have a Decode fallback.
  static rive::rcp<rive::Font> DecodeFile(
      const char* path,
      unsigned faceIndex = 0);
  static rive::rcp<rive::Font> FromSystem(
      void* systemFont,
      bool useSystemShaper,
      uint16_t weight,
      uint8_t width);
  static float GetStyle(hb_font_t*, uint32_t);
  hb_font_t* font() const { return m_font; }
#ifdef TESTING
  // Variants made, rather than shared, and glyph outlines drawn rather than
  // found in a cache, across every font and thread.
  static std::atomic<uint64_t> sm_variantsMade;
  static std::atomic<uint64_t> sm_glyphPathsDrawn;
  // Called by a variant's destructor once its count reached zero, before it
  // leaves its font's map, so a test can look the variant up while it dies.
  static void (*sm_variantDying)();
  // Live variants sharedWithOptions compared a request's options against.
  static std::atomic<uint64_t> sm_variantsCompared;
#endif

private:
  HBFont(
      hb_font_t* font,
      std::unordered_map<uint32_t, float> axisValues,
      std::unordered_map<uint32_t, uint32_t> featureValues,
      std::vector<hb_feature_t> features);
  // With [lineMetrics] rather than measuring [font], for a copy, which
  // measures as the font it copies does.
  HBFont(
      hb_font_t* font,
      const LineMetrics& lineMetrics,
      std::unordered_map<uint32_t, float> axisValues,
      std::unordered_map<uint32_t, uint32_t> featureValues,
      std::vector<hb_feature_t> features);

  // withOptions, or sharedWithOptions when [shared].
  rive::rcp<Font> makeVariant(
      rive::Span<const Coord> variableAxes,
      rive::Span<const Feature> features,
      bool shared) const;

public:
  hb_font_t* m_font;
  virtual void shapeFallbackRun(
      rive::SimpleArrayBuilder<rive::GlyphRun>& gruns,
      const rive::Unichar text[],
      const unsigned textStart,
      const rive::TextRun& textRun,
      const rive::TextRun& originalTextRun,
      const uint32_t fallbackIndex);

  // Draws [glyph]'s outline. glyphPath (and getPath) keep what this returns,
  // so a font that draws its outlines elsewhere overrides this instead.
  virtual rive::RawPath drawGlyphPath(rive::GlyphID glyph) const;

  // The features list to pass directly to Harfbuzz.
  std::vector<hb_feature_t> m_features;

private:
  hb_draw_funcs_t* m_drawFuncs;
  hb_paint_funcs_t* m_paintFuncs;

  // Feature value lookup based on tag.
  std::unordered_map<uint32_t, uint32_t> m_featureValues;

  // Axis value lookup based on for the feature.
  std::unordered_map<uint32_t, float> m_axisValues;

  // Color glyph (emoji) support — COLRv0 (layers), COLRv1 (paint),
  // and/or SBIX/CBDT (PNG bitmap).
  bool m_hasColorLayers = false; // COLRv0
  bool m_hasColorPaint = false;  // COLRv1
  bool m_hasPNG = false;         // SBIX or CBDT
  std::vector<hb_color_t> m_paletteColors;
  mutable std::unordered_map<rive::GlyphID, std::vector<ColorGlyphLayer>>
      m_colorLayerCache;
  // Outlines already drawn, so a text that reshapes (or a list row made for
  // a new item) reuses its glyphs instead of walking the glyf table again.
  // Bounded; it starts over once full.
  mutable std::unordered_map<rive::GlyphID, rive::RawPath> m_glyphPathCache;
  // Past this many outlines, glyphPath starts the cache over.
  static constexpr size_t kMaxCachedGlyphPaths = 512;

  // The fonts sharedWithOptions made from this one that are still alive, by
  // variantKey. A request for options one of them already has, from the
  // thread that made it, gets that font, and with it the outlines it already
  // drew: every list row's text style asks for the same variable font, and
  // each used to build its own and draw every glyph again. Keyed rather than
  // listed because a variation modifier's falloff keeps a variant alive per
  // coverage level, thousands on a long text. Not owned: a variant holds
  // this font (m_variantOf) and takes itself out as it is destroyed. Fonts
  // withOptions makes aren't listed, so one its caller hands to another
  // thread is never handed out again. The copies forThisThread made are
  // listed too, each keyed by its thread.
  mutable std::mutex m_variantsMutex;
  mutable std::unordered_multimap<uint64_t, HBFont*> m_variants;
  // The font sharedWithOptions or forThisThread made this one from, if one
  // did, and this one's key in that font's m_variants.
  rive::rcp<const HBFont> m_variantOf;
  uint64_t m_variantKey = 0;
  // The thread sharedWithOptions or forThisThread made this one on, as a
  // number no later thread gets (a std::thread::id can be reused once its
  // thread ends). Its caches aren't locked, as a base font's aren't, so it
  // is only shared on that thread. One base font can serve files that
  // advance on different threads, whose styles each get a font of their own.
  uint64_t m_madeOn = 0;
  // The thread forThisThread first asked on, which keeps this font, or zero.
  mutable std::atomic<uint64_t> m_firstThread{0};
  // Whether forThisThread made this font, a copy of m_variantOf.
  bool m_isCopy = false;
};

#endif
