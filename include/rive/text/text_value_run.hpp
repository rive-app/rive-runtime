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
    TextStyle* style() { return m_style; }

protected:
    void textChanged() override;

private:
    TextStyle* m_style = nullptr;
};
} // namespace rive

#endif