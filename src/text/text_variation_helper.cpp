#include "rive/text/text_variation_helper.hpp"
#include "rive/text/text_style.hpp"
#include "rive/artboard.hpp"

using namespace rive;

void TextVariationHelper::buildDependencies()
{
  auto parent = m_textStyle->parent();
  // A global text style's paint lives under its style Artboard, with no Text
  // to re-shape here (its owner re-shapes the Texts drawing it). Wiring the
  // artboard on both sides would be a cycle.
  if (parent->is<Artboard>())
  {
    parent->addDependent(this);
    return;
  }
  parent->artboard()->addDependent(this);
  addDependent(parent);
}

void TextVariationHelper::update(ComponentDirt value)
{
  m_textStyle->updateVariableFont();
}