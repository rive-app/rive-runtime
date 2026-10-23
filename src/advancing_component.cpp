#include "rive/component.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/artboard.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/layout_component.hpp"
#include "rive/layout/layout_participant.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/nested_artboard_layout.hpp"
#include "rive/nested_artboard_leaf.hpp"
#include "rive/scripted/scripted_data_converter.hpp"
#include "rive/scripted/scripted_drawable.hpp"
#include "rive/scripted/scripted_layout.hpp"
#include "rive/scripted/scripted_transition.hpp"
#include "rive/scripted/scripted_path_effect.hpp"
#include "rive/shapes/paint/paint_image.hpp"
#include "rive/text/text_input.hpp"
#include "rive/text/text_style_global.hpp"
#include "rive/text/text.hpp"

using namespace rive;

AdvancingComponent* AdvancingComponent::from(Core* component)
{
  switch (component->coreType())
  {
    case NestedArtboardLeaf::typeKey:
    case NestedArtboardLayout::typeKey:
    case NestedArtboard::typeKey:
      return component->as<NestedArtboard>();
    case LayoutComponent::typeKey:
      return component->as<LayoutComponent>();
    case LayoutParticipantBase::typeKey:
      return component->as<LayoutParticipant>();
    case Artboard::typeKey:
      return component->as<Artboard>();
    case ArtboardComponentListBase::typeKey:
      return component->as<ArtboardComponentList>();
    case ScrollConstraint::typeKey:
      return component->as<ScrollConstraint>();
    case TextInputBase::typeKey:
      return component->as<TextInput>();
    case TextStyleGlobalBase::typeKey:
      return component->as<TextStyleGlobal>();
    case TextBase::typeKey:
      // Advances global text styles instanced for its list runs.
      return component->as<Text>();
    case ScriptedDataConverter::typeKey:
      return component->as<ScriptedDataConverter>();
    case ScriptedDrawable::typeKey:
      return component->as<ScriptedDrawable>();
    case ScriptedLayout::typeKey:
      return component->as<ScriptedLayout>();
    case ScriptedTransition::typeKey:
      return component->as<ScriptedTransition>();
    case ScriptedPathEffect::typeKey:
      return component->as<ScriptedPathEffect>();
    case PaintImageBase::typeKey:
      return component->as<PaintImage>();
  }
  return nullptr;
}