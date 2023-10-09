#ifndef _RIVE_TEXT_VALUE_RUN_HPP_
#define _RIVE_TEXT_VALUE_RUN_HPP_
#include "rive/generated/text/text_value_run_base.hpp"

namespace rive
{
class TextStyle;
class TextValueRun : public TextValueRunBase
{
public:
    StatusCode onAddedClean(CoreContext* context) override;
    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode import(ImportStack& importStack) override;
    TextStyle* style() { return m_style; }
    uint32_t offset() const;

protected:
    void textChanged() override;
    void styleIdChanged() override;

private:
    TextStyle* m_style = nullptr;
};
} // namespace rive

#endif