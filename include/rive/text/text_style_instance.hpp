#ifndef _RIVE_TEXT_STYLE_INSTANCE_HPP_
#define _RIVE_TEXT_STYLE_INSTANCE_HPP_
#include "rive/advance_flags.hpp"
#include "rive/advancing_component.hpp"
#include "rive/refcnt.hpp"
#include "rive/text/text_style.hpp"
#include <memory>
#include <string>
#include <vector>

namespace rive
{
class Artboard;
class ArtboardInstance;
class DataContext;
class Factory;
class StateMachineInstance;
class Text;
class TextInterface;
class TextStylePaint;
class TextStyleReference;
class ViewModelInstance;
class ViewModelInstanceValue;

/// One Text's own instance of a global text style: the style artboard
/// instanced for it, bound to a view model instance (the Text's exposed
/// values, a bound one, or the style's defaults) under the data context of
/// the artboard hosting the Text, so its binds read their own view model
/// first, then the host's data, then globals. Runs the style's default state
/// machine, one for every run drawn with it. Shared by TextStyleGlobal handles
/// and on-demand list styles; a style every Text can share uses a
/// SharedTextStyle instead.
class TextStyleInstance : public TextStyleOwner
{
public:
  /// With [servedText], the style's state machine listens to the runs of
  /// that Text drawn with [servedStyle] (see
  /// StateMachineInstance::serveTextStyle).
  TextStyleInstance(
      const Artboard* source,
      Factory* factory,
      rcp<ViewModelInstance> viewModelInstance,
      bool readsOutsideOwnViewModel,
      TextInterface* text,
      Text* servedText = nullptr,
      const TextStyleReference* servedStyle = nullptr);
  ~TextStyleInstance() override;

  ArtboardInstance* artboardInstance() const { return m_instance.get(); }
  TextStylePaint* paint() const { return m_paint; }
  const rcp<ViewModelInstance>& viewModelInstance() const
  {
    return m_viewModelInstance;
  }
  StateMachineInstance* stateMachine() const { return m_stateMachine.get(); }

  /// Binds the style to its view model instance under [parent] (the hosting
  /// artboard's data context, null while it has none). A no-op when already
  /// bound under [parent], unless [force]d or the style reads the host's
  /// data (the host may have swapped the instances in [parent]).
  void bind(rcp<DataContext> parent, bool force = false);
  /// Rebinds after the host's data context relinked, if anything here reads
  /// past the style's own view model.
  void relink();
  /// The list item of the one list run this instance is drawn for, if any:
  /// its binds and state machine then read the style's own view model, then
  /// the item's, then the host's data.
  void listItem(rcp<ViewModelInstance> value);
  /// Copies [source]'s values (a Text handle's) into this instance's own
  /// view model instance, and keeps its exposed inputs following [source]
  /// (see syncInputs). For a list item's own instance of a style: what its
  /// state machine writes to the style's values stays its own.
  void copyValuesFrom(rcp<ViewModelInstance> source);
  /// Copies the exposed inputs that changed on the values source since the
  /// last copy. Returns whether any did.
  bool syncInputs();

  /// Applies the style's binds; when its values changed, flushes its graph
  /// and re-shapes the Text. Returns whether anything changed.
  bool flush();
  /// Advances the style's state machine and artboard, then [flush]es.
  /// Returns whether it wants to keep advancing.
  bool advance(float elapsedSeconds, AdvanceFlags flags);
  AdvancingComponent::QuietState quietState() const;

  // TextStyleOwner
  void styleShapeChanged() override;

private:
  /// An exposed input copied from the values source to this instance's own
  /// view model instance, with the source's value at the last copy.
  struct InputLink
  {
    rcp<ViewModelInstanceValue> from;
    rcp<ViewModelInstanceValue> to;
    float number = 0.0f;
    uint32_t index = 0;
    int color = 0;
    bool boolean = false;
    std::string string;
  };

  // Destroyed before the rest (see the destructor).
  rcp<ViewModelInstance> m_valuesSource;
  std::vector<InputLink> m_inputs;
  std::unique_ptr<ArtboardInstance> m_instance;
  std::unique_ptr<StateMachineInstance> m_stateMachine;
  rcp<ViewModelInstance> m_viewModelInstance;
  rcp<ViewModelInstance> m_listItem;
  rcp<DataContext> m_parent;
  TextStylePaint* m_paint = nullptr;
  TextInterface* m_text = nullptr;
  bool m_readsOutsideOwnViewModel = false;
  bool m_isBound = false;
};

} // namespace rive

#endif
