#include "rive/text/text_variation_helper.hpp"
#include "rive/text/text_style.hpp"
#include "rive/artboard.hpp"

using namespace rive;

void TextVariationHelper::buildDependencies()
{
    auto text = m_textStyle->parent();
    text->artboard()->addDependent(this);
    addDependent(text);
}

void TextVariationHelper::update(ComponentDirt value)
{
    m_textStyle->updateVariableFont();
}