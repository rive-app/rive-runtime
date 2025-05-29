#ifndef _RIVE_TEXT_VALUE_RUN_HPP_
#define _RIVE_TEXT_VALUE_RUN_HPP_
#include "rive/generated/text/text_value_run_base.hpp"
#include "rive/animation/hittable.hpp"
#include "rive/text/utf.hpp"
#include "rive/math/rectangles_to_contour.hpp"

namespace rive
{
class TextStylePaint;
class Text;
class TextValueRun : public TextValueRunBase, public Hittable
{
    friend class HitTextRun;

public:
    StatusCode onAddedClean(CoreContext* context) override;
    StatusCode onAddedDirty(CoreContext* context) override;
    TextStylePaint* style() { return m_style; }
    Text* textComponent() const;
    uint32_t length()
    {
        if (m_length == -1)
        {

            const uint8_t* ptr = (const uint8_t*)text().c_str();
            uint32_t n = 0;
            while (*ptr)
            {
                UTF::NextUTF8(&ptr);
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

    bool hitTestAABB(const Vec2D& position) override;
    bool hitTestHiFi(const Vec2D& position, float hitRadius) override;

    bool isHitTarget() const { return m_isHitTarget; }
    void isHitTarget(bool value);

protected:
    void textChanged() override;
    void styleIdChanged() override;

private:
    std::unique_ptr<RectanglesToContour> m_rectanglesToContour;
    AABB m_localBounds;
    bool m_isHitTarget = false;
    std::vector<AABB> m_glyphHitRects;
    TextStylePaint* m_style = nullptr;
    uint32_t m_length = -1;
    bool canHitTest() const;
};
} // namespace rive

#endif
