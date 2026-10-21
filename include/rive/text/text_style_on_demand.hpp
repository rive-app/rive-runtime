#ifndef _RIVE_TEXT_STYLE_ON_DEMAND_HPP_
#define _RIVE_TEXT_STYLE_ON_DEMAND_HPP_
#include "rive/text/text_style_reference.hpp"
#include "rive/advance_flags.hpp"
#include "rive/advancing_component.hpp"
#include "rive/refcnt.hpp"
#include "rive/text/text_style_share_cache.hpp"
#include <memory>

namespace rive
{
class Artboard;
class DataContext;
class Factory;
class File;
class Text;
class TextStyleInstance;
class TextStylePaint;
class ViewModelInstance;

/// A global text style a [Text] uses for its list-driven runs when an item
/// asks for a style the Text has no [TextStyleGlobal] handle showing. Like a
/// handle it draws the File's shared instance of a style every Text can
/// share, or else instances the style for this Text (see TextStyleInstance),
/// bound to a view model instance under the Text's artboard's data context.
/// Owned by the Text: never parented, never added to an artboard. One per
/// style (and item-supplied view model instance) per Text, shared by all its
/// runs.
class TextStyleOnDemand : public TextStyleReference
{
public:
  /// [source] is the file-owned style artboard. [file] owns it and its view
  /// model; [crossFile] is set instead when the style comes from another
  /// file (a runtime-set BindableArtboard), and is kept alive here.
  /// [parent] is the hosting artboard's data context. A list [item]'s own
  /// instance copies [valuesSource] (the Text's handle values for the
  /// style), keeping its exposed inputs in step (see
  /// TextStyleInstance::copyValuesFrom).
  TextStyleOnDemand(
      const Artboard* source,
      rcp<ViewModelInstance> boundViewModelInstance,
      File* file,
      rcp<const File> crossFile,
      Factory* factory,
      rcp<DataContext> parent,
      Text* text,
      rcp<ViewModelInstance> item = nullptr,
      rcp<ViewModelInstance> valuesSource = nullptr);
  ~TextStyleOnDemand() override;

  const Artboard* source() const { return m_source; }
  const ViewModelInstance* boundViewModelInstance() const
  {
    return m_boundViewModelInstance.get();
  }
  /// The list item this instance was made for alone (a style with a state
  /// machine keeps one instance per item), or null when every list run
  /// naming the style shares it.
  const ViewModelInstance* item() const { return m_item.get(); }
  bool isShared() const { return (bool)m_shared; }
  /// This style's own instance, unless it draws the shared one.
  TextStyleInstance* instance() const { return m_instance.get(); }

  /// The hosting artboard's data context changed (or relinked).
  void hostDataContext(rcp<DataContext> parent);
  void relinkHostDataContext();
  /// Applies the style's binds after the host's; returns whether its paint
  /// changed.
  bool updateHostedDataBinds();

  /// Advances the style's state machine and data binds and flushes its
  /// component graph. Returns whether it wants to keep advancing; sets
  /// [changed] when the paint may have changed.
  bool advance(float elapsedSeconds, AdvanceFlags flags, bool& changed);
  AdvancingComponent::QuietState quietState() const;
  TextStylePaint* resolvedPaint() override { return m_paint; }
  bool listensToRuns() const override;
  StateMachineInstance* runStateMachine() const override;
  void listItem(const rcp<ViewModelInstance>& value) override;

private:
  const Artboard* m_source = nullptr;
  TextStyleShareInfo m_info;
  rcp<const File> m_crossFile;
  // The item-supplied instance (part of this style's identity), if any.
  rcp<ViewModelInstance> m_boundViewModelInstance;
  // See item(); part of this style's identity.
  rcp<ViewModelInstance> m_item;
  std::unique_ptr<TextStyleInstance> m_instance;
  SharedTextStyleLease m_shared;
  TextStylePaint* m_paint = nullptr;
};
} // namespace rive

#endif
