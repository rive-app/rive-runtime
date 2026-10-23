#ifndef _RIVE_TEXT_STYLE_REFERENCE_HPP_
#define _RIVE_TEXT_STYLE_REFERENCE_HPP_
#include "rive/generated/text/text_style_reference_base.hpp"
#include "rive/refcnt.hpp"
#include <memory>
namespace rive
{
class StateMachineInstance;
class TextStylePaint;
class ViewModelInstance;

/// Shared base for anything a [TextValueRun] can reference as its style: both
/// local [TextStylePaint]s and [TextStyleGlobal] handles. [resolvedPaint] is
/// the single hop used to obtain the concrete paint this reference shapes and
/// paints with.
class TextStyleReference : public TextStyleReferenceBase
{
public:
  /// The concrete [TextStylePaint] used to shape/paint glyphs for this
  /// reference. Local styles return themselves; a global style resolves the
  /// paint inside its instanced artboard. Returns null when unresolved.
  virtual TextStylePaint* resolvedPaint() { return nullptr; }

  /// Whether this style's state machine listens to the runs drawn with it
  /// (pointer listeners with no target, see TextStyleShareInfo).
  virtual bool listensToRuns() const { return false; }
  /// The state machine listening to the runs drawn with this style, one
  /// for all of them: hovering or pressing any of them restyles them all
  /// (see StateMachineInstance::serveTextStyle). Null when none listens.
  virtual StateMachineInstance* runStateMachine() const { return nullptr; }
  /// The list item of the one list run drawn with this style, if any (see
  /// TextStyleInstance::listItem). Set by the Text.
  /// (By reference: the type is only declared here.)
  virtual void listItem(const rcp<ViewModelInstance>& value) {}
};
} // namespace rive

#endif