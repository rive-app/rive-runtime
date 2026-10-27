#ifndef _RIVE_TEXT_VALUE_RUN_HPP_
#define _RIVE_TEXT_VALUE_RUN_HPP_
#include "rive/generated/text/text_value_run_base.hpp"
#include "rive/text/utf.hpp"
#include "rive/text/text_style_reference.hpp"
#include "rive/math/rectangles_to_contour.hpp"
#include "rive/refcnt.hpp"

namespace rive
{
class TextStylePaint;
class Text;
class ViewModelInstanceArtboard;
class TextValueRun : public TextValueRunBase
{
  friend class HitTextRun;

public:
  // Out of line: m_boundStyleValue's type is only forward declared here.
  TextValueRun();
  ~TextValueRun() override;
  StatusCode onAddedClean(CoreContext* context) override;
  StatusCode onAddedDirty(CoreContext* context) override;
  // Resolves lazily so a global style whose nested-artboard instance is
  // created after this run is added still returns the current paint.
  TextStylePaint* style()
  {
    return m_styleReference != nullptr ? m_styleReference->resolvedPaint()
                                       : nullptr;
  }
  void style(TextStyleReference* value) { m_styleReference = value; }
  TextStyleReference* styleReference() const { return m_styleReference; }
  /// Draws the global text style [value] (a text style view model value)
  /// names in place of the authored one, while a data bind on styleId holds
  /// it (see Text::boundRunStyle). styleId keeps the authored style, which a
  /// value naming no text style falls back to.
  void bindStyle(ViewModelInstanceArtboard* value);
  bool isStyleBound() const { return m_boundStyleValue != nullptr; }
  /// Resolves the style the run draws again: the bound one, else the
  /// authored one.
  void resolveStyle();
  Text* textComponent() const;
  void textComponent(Text* value) { m_textComponent = value; };
  uint32_t length()
  {
    if (m_length == -1)
    {

      const uint8_t* ptr = (const uint8_t*)text().data();
      const uint8_t* end = ptr + text().size();
      uint32_t n = 0;
      while (ptr < end && *ptr)
      {
        UTF::NextUTF8(&ptr, end);
        n += 1;
      }
      m_length = n;
    }
    return m_length;
  }
  uint32_t offset() const;

  // Reset stored data for hit testing the run.
  void resetHitTest();

  // Add a rectangle (usually bounding a glyph) as a hit rect that will be
  // used to compute contours when computeHitContours is called.
  void addHitRect(const AABB& rect);

  // Compute the contours used for hit testing, call resetHitTest to start
  // adding hit rects (via addHitRect) again.
  void computeHitContours();

  bool hitTestAABB(const Vec2D& position);
  bool hitTestHiFi(const Vec2D& position, float hitRadius);

  bool isHitTarget() const { return m_isHitTarget; }
  void isHitTarget(bool value);
  bool hitTestPoint(
      const Vec2D& position,
      bool skipOnUnclipped,
      bool isPrimaryHit) override;

protected:
  void textChanged() override;
  void styleIdChanged() override;

private:
  std::unique_ptr<RectanglesToContour> m_rectanglesToContour;
  AABB m_localBounds;
  bool m_isHitTarget = false;
  std::vector<AABB> m_glyphHitRects;
  TextStyleReference* m_styleReference = nullptr;
  // The text style view model value a data bind on styleId holds, if any.
  rcp<ViewModelInstanceArtboard> m_boundStyleValue;
  uint32_t m_length = -1;
  bool canHitTest() const;
  TextStyleReference* authoredStyle() const;
  Text* m_textComponent = nullptr;
};
} // namespace rive

#endif
