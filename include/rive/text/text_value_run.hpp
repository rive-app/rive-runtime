#ifndef _RIVE_TEXT_VALUE_RUN_HPP_
#define _RIVE_TEXT_VALUE_RUN_HPP_
#include "rive/generated/text/text_value_run_base.hpp"
#include "rive/text/utf.hpp"

namespace rive
{
class TextStyle;
class TextValueRun : public TextValueRunBase
{
public:
    StatusCode onAddedClean(CoreContext* context) override;
    StatusCode onAddedDirty(CoreContext* context) override;
    TextStyle* style() { return m_style; }
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

protected:
    void textChanged() override;
    void styleIdChanged() override;

private:
    TextStyle* m_style = nullptr;
    uint32_t m_length = -1;
};
} // namespace rive

#endif