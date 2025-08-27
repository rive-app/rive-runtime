#ifndef _RIVE_TEXT_VARIATION_HELPER_HPP_
#define _RIVE_TEXT_VARIATION_HELPER_HPP_

#include "rive/component.hpp"
namespace rive
{
class TextStyle;
class TextVariationHelper : public Component
{
public:
    TextVariationHelper(TextStyle* style) : m_textStyle(style) {}
    TextStyle* style() const { return m_textStyle; }
    void buildDependencies() override;
    void update(ComponentDirt value) override;

private:
    TextStyle* m_textStyle;
};
} // namespace rive

#endif
