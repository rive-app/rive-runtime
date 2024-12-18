#ifndef _RIVE_TEXT_VALUE_RUN_HPP_
#define _RIVE_TEXT_VALUE_RUN_HPP_
#include "rive/generated/text/text_value_run_base.hpp"
#include "rive/animation/hittable.hpp"
#include "rive/text/utf.hpp"

namespace rive
{
class TextStyle;
class Text;
class TextValueRun : public TextValueRunBase, public Hittable
{
public:
    StatusCode onAddedClean(CoreContext* context) override;
    StatusCode onAddedDirty(CoreContext* context) override;
    TextStyle* style() { return m_style; }
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

    // Hit testing
    AABB m_localBounds;
    std::vector<std::vector<Vec2D>> m_contours;
    bool m_isHitTarget = false;
    void resetHitTest(); // clear m_contours and m_localBounds
    bool hitTestAABB(const Vec2D& position) override;
    bool hitTestHiFi(const Vec2D& position, float hitRadius) override;

protected:
    void textChanged() override;
    void styleIdChanged() override;

private:
    TextStyle* m_style = nullptr;
    uint32_t m_length = -1;
    bool canHitTest() const;
};
} // namespace rive

#endif
