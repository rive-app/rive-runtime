#ifndef _RIVE_TEXT_STYLE_GLOBAL_HPP_
#define _RIVE_TEXT_STYLE_GLOBAL_HPP_
#include "rive/generated/text/text_style_global_base.hpp"
#include "rive/advancing_component.hpp"
#include "rive/artboard_referencer.hpp"
#include "rive/data_bind/host_data_context_consumer.hpp"
#include "rive/refcnt.hpp"
#include "rive/text/text_style_share_cache.hpp"
#include <memory>

namespace rive
{
class TextStylePaint;
class TextStyleInstance;
class ArtboardInstance;
class DataContext;
class File;
class ViewModelInstance;
class ViewModelInstanceArtboard;
class ImportStack;

/// A handle, living as a child of a [Text], that references a global text
/// style (an artboard of kind textStyle) via [artboardId]. At runtime it
/// instances that artboard for its Text (see TextStyleInstance): bound to its
/// child ViewModelInstance (the Text's exposed values) under the hosting
/// artboard's data context, advanced each frame so its [TextStylePaint]
/// reflects the exposed and data-bound values. A style with no state machine
/// logic, no exposed properties and binds reading only its own view model is
/// the same for every Text, so the handle draws the File's single shared
/// instance of it instead (see TextStyleShareCache).
///
/// [artboardId] is bindable to a text-style view model property (an artboard
/// property at runtime): [updateArtboard] then re-instances whichever style
/// artboard the property names, so every run pointing at this handle restyles.
class TextStyleGlobal : public TextStyleGlobalBase,
                        public ArtboardReferencer,
                        public AdvancingComponent,
                        public HostDataContextConsumer
{
public:
  TextStyleGlobal();
  ~TextStyleGlobal() override;

  StatusCode import(ImportStack& importStack) override;
  Core* clone() const override;
  StatusCode onAddedClean(CoreContext* context) override;

  // ArtboardReferencer
  int referencedArtboardId() override;
  void referencedArtboard(Artboard* artboard) override;
  void updateArtboard(
      ViewModelInstanceArtboard* viewModelInstanceArtboard) override;

  // AdvancingComponent
  bool advanceComponent(float elapsedSeconds, AdvanceFlags flags) override;
  QuietState quietState() override;

  // HostDataContextConsumer
  void hostDataContext(rcp<DataContext> value) override;
  void relinkHostDataContext() override;
  void updateHostedDataBinds() override;

  void file(File* value) { m_file = value; }
  File* file() const { return m_file; }

  /// The instanced style artboard this handle draws: its own, or the shared
  /// one. Null on the file's own (non-instance) host.
  ArtboardInstance* artboardInstance() const;
  /// Whether this handle draws the File's shared instance of its style.
  bool isShared() const { return (bool)m_shared; }
  /// The view model instance this handle's own instance of its style reads
  /// (its exposed values, a bound one, or the style's defaults). Null when
  /// it draws the shared instance.
  rcp<ViewModelInstance> styleViewModelInstance() const;

  TextStylePaint* resolvedPaint() override;
  // A style listening to its runs listens to all the runs drawn with this
  // handle, as one: they share its instance.
  bool listensToRuns() const override;
  StateMachineInstance* runStateMachine() const override;
  void listItem(const rcp<ViewModelInstance>& value) override;

  // The first TextStylePaint of a style artboard; null when the artboard is
  // not a text style.
  static TextStylePaint* firstTextStylePaint(const Artboard* artboard);

  /// The file-owned style artboard this handle currently shows (the
  /// authored one, or the one a binding swapped in). Null on the file's own
  /// (non-instance) host.
  const Artboard* instanceSource() const { return m_instanceSource; }

private:
  /// Instances (or shares) [source] for this handle's Text, bound to
  /// [bound] if given, else the child instance when it holds values for
  /// [source]'s view model, else the style's defaults.
  void acquireStyle(
      const Artboard* source,
      rcp<ViewModelInstance> bound,
      rcp<const File> sourceFile);
  void releaseStyle();

  std::unique_ptr<TextStyleInstance> m_instance;
  SharedTextStyleLease m_shared;
  // The paint this handle resolves to; cached so runs don't search for it
  // per glyph.
  TextStylePaint* m_paint = nullptr;
  // What the file knows about the drawn style.
  TextStyleShareInfo m_info;
  File* m_file = nullptr;
  // The file-owned style artboard this handle was authored against. A
  // binding that clears the style reverts to it.
  Artboard* m_sourceArtboard = nullptr;
  // The file-owned artboard the drawn instance was created from.
  const Artboard* m_instanceSource = nullptr;
  // Borrowed: the handle's child ViewModelInstance, owned by the parent
  // artboard's object list. Only matches the authored style's view model.
  ViewModelInstance* m_childViewModelInstance = nullptr;
  // A view model instance supplied through the bound property.
  rcp<ViewModelInstance> m_boundViewModelInstance;
  // Kept alive while showing a runtime-set style owned by another file.
  rcp<const File> m_crossFile;
  // The hosting artboard's data context, the parent of the style's own.
  rcp<DataContext> m_hostDataContext;
  // See listItem.
  rcp<ViewModelInstance> m_listItem;
};
} // namespace rive

#endif
