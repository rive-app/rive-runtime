#include "rive/text/text_style_global.hpp"
#include "rive/artboard.hpp"
#include "rive/backboard.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/file.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/refcnt.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_interface.hpp"
#include "rive/text/text_style_instance.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"

using namespace rive;

TextStyleGlobal::TextStyleGlobal() {}

TextStyleGlobal::~TextStyleGlobal()
{
  // Destroy the instanced style artboard (and its data binds, which
  // reference the active VMI) while that VMI is still alive: the borrowed
  // child is owned by the parent artboard's object list, the bound one by
  // the rcp released below.
  releaseStyle();
  m_boundViewModelInstance = nullptr;
  m_hostDataContext = nullptr;
}

StatusCode TextStyleGlobal::import(ImportStack& importStack)
{
  auto backboardImporter = importStack.latest<BackboardImporter>(
      Backboard::typeKey);
  if (backboardImporter == nullptr)
  {
    return StatusCode::MissingObject;
  }
  // Registers this referencer; resolved to the source artboard by id in
  // BackboardImporter::resolve() once all artboards are imported.
  backboardImporter->addArtboardReferencer(this);
  return Super::import(importStack);
}

int TextStyleGlobal::referencedArtboardId() { return artboardId(); }

TextStylePaint* TextStyleGlobal::firstTextStylePaint(const Artboard* artboard)
{
  if (artboard == nullptr)
  {
    return nullptr;
  }
  for (Core* object : artboard->objects())
  {
    if (object != nullptr && object->is<TextStylePaint>())
    {
      return static_cast<TextStylePaint*>(object);
    }
  }
  return nullptr;
}

void TextStyleGlobal::releaseStyle()
{
  m_paint = nullptr;
  m_instance = nullptr;
  m_shared.release();
  m_instanceSource = nullptr;
}

void TextStyleGlobal::acquireStyle(
    const Artboard* source,
    rcp<ViewModelInstance> bound,
    rcp<const File> sourceFile)
{
  Artboard* host = artboard();
  auto text = TextInterface::from(parent());
  if (m_instanceSource != nullptr && text != nullptr)
  {
    // Let the Text drop its pointers into the outgoing paint while it is
    // still alive.
    text->styleReferenceWillChange();
  }
  releaseStyle();
  m_info = TextStyleShareInfo();
  m_boundViewModelInstance = bound;
  // A runtime-set style from another file: keep that file (which owns the
  // style artboard and its view model) alive while instancing it.
  bool ownedByFile = sourceFile == nullptr || sourceFile.get() == m_file;
  m_crossFile = ownedByFile ? nullptr : sourceFile;
  if (source == nullptr || host == nullptr)
  {
    return;
  }
  Factory* factory = host->factory();
  m_instanceSource = source;
  const File* viewModelFile = ownedByFile ? m_file : m_crossFile.get();

  TextStyleShareInfo info;
  if (m_file != nullptr && ownedByFile)
  {
    info = m_file->textStyleShares().info(source);
  }
  else
  {
    info = TextStyleShareCache::analyze(source, viewModelFile);
  }
  m_info = info;

  // A style every Text draws the same way: draw the shared instance.
  if (bound == nullptr && info.shareable && text != nullptr)
  {
    m_shared = m_file->textStyleShares().acquire(source, factory, text);
    if (m_shared)
    {
      m_paint = m_shared.get()->paint();
      return;
    }
  }

  rcp<ViewModelInstance> viewModelInstance = bound;
  if (viewModelInstance == nullptr && m_childViewModelInstance != nullptr &&
      m_childViewModelInstance->viewModelId() == source->viewModelId())
  {
    // The authored style (or another style sharing its view model) keeps
    // the per-handle exposed values.
    viewModelInstance = ref_rcp(m_childViewModelInstance);
  }
  if (viewModelInstance == nullptr && viewModelFile != nullptr)
  {
    auto viewModel = viewModelFile->viewModel(source->viewModelId());
    if (viewModel != nullptr)
    {
      viewModelInstance = viewModelFile->createDefaultViewModelInstance(
          viewModel);
      if (viewModelInstance != nullptr && m_file != nullptr && ownedByFile)
      {
        m_file->completeViewModelProperties(viewModelInstance.get());
      }
    }
  }
  // A style listening to its runs listens to every run drawn with this
  // handle, as one.
  Text* servedText = info.listensToRuns && parent() != nullptr &&
                             parent()->is<Text>()
                         ? parent()->as<Text>()
                         : nullptr;
  m_instance = std::make_unique<TextStyleInstance>(
      source,
      factory,
      viewModelInstance,
      info.readsOutsideOwnViewModel,
      text,
      servedText,
      this);
  m_paint = m_instance->paint();
  m_instance->listItem(m_listItem);
  m_instance->bind(m_hostDataContext);
  // Shape against the bound values from the first frame. (No re-shape: the
  // Text may still be initializing; a swap re-shapes it explicitly.)
  auto instance = m_instance->artboardInstance();
  instance->updateDataBinds();
  instance->updatePass(false);
}

void TextStyleGlobal::updateArtboard(
    ViewModelInstanceArtboard* viewModelInstanceArtboard)
{
  // Only host instances swap; the file's source artboard keeps referencing
  // the authored style and owns no instance.
  Artboard* host = artboard();
  if (viewModelInstanceArtboard == nullptr || host == nullptr ||
      !host->isInstance())
  {
    return;
  }

  // asset == nullptr and propertyValue == -1 is the property explicitly set
  // to no artboard. A run can't shape without a paint, so revert to the
  // authored style instead of clearing it.
  bool explicitNull = viewModelInstanceArtboard->asset() == nullptr &&
                      viewModelInstanceArtboard->propertyValue() == -1;
  Artboard* target = explicitNull ? m_sourceArtboard
                                  : findArtboard(
                                        viewModelInstanceArtboard,
                                        host,
                                        m_file);
  // An unresolvable id, or an artboard that isn't a text style, keeps the
  // current style rather than leaving the runs without a paint.
  if (target == nullptr || firstTextStylePaint(target) == nullptr)
  {
    return;
  }

  // A runtime-set value is a BindableArtboard wrapping its own instance;
  // instance from the file-owned source so the style never depends on that
  // wrapper's lifetime.
  const Artboard* source = target->artboardSource();
  rcp<ViewModelInstance> bound = explicitNull ? nullptr
                                              : viewModelInstanceArtboard
                                                    ->boundViewModelInstance();
  rcp<const File> sourceFile;
  if (!explicitNull && viewModelInstanceArtboard->asset() != nullptr &&
      target->isInstance())
  {
    sourceFile = static_cast<ArtboardInstance*>(target)->file();
  }
  // The bind applies on every initial bind; don't rebuild the same style.
  if (m_paint != nullptr && m_instanceSource == source &&
      m_boundViewModelInstance == bound)
  {
    return;
  }

  acquireStyle(source, bound, sourceFile);
  addDirt(ComponentDirt::Components);
  if (auto text = TextInterface::from(parent()))
  {
    // Re-shapes, and lets a Text re-resolve list runs that may have
    // matched this handle by style.
    text->styleReferenceDidChange();
  }
}

void TextStyleGlobal::referencedArtboard(Artboard* artboard)
{
  m_referencedArtboard = artboard;
  m_sourceArtboard = artboard;
}

Core* TextStyleGlobal::clone() const
{
  auto clone = static_cast<TextStyleGlobal*>(TextStyleGlobalBase::clone());
  clone->file(file());
  // The clone instances (or shares) the authored style once it's added to
  // its host instance (see onAddedClean).
  clone->referencedArtboard(m_sourceArtboard);
  return clone;
}

StatusCode TextStyleGlobal::onAddedClean(CoreContext* context)
{
  // The child ViewModelInstance (authored in the editor, cloned into each
  // instance) holds this handle's exposed values for the authored style.
  for (auto child : children())
  {
    if (child->is<ViewModelInstance>())
    {
      m_childViewModelInstance = child->as<ViewModelInstance>();
      break;
    }
  }
  auto code = Super::onAddedClean(context);
  Artboard* host = artboard();
  if (host != nullptr && host->isInstance() && m_sourceArtboard != nullptr)
  {
    if (m_childViewModelInstance != nullptr && m_file != nullptr)
    {
      m_file->completeViewModelProperties(m_childViewModelInstance);
    }
    acquireStyle(m_sourceArtboard, nullptr, nullptr);
  }
  return code;
}

void TextStyleGlobal::hostDataContext(rcp<DataContext> value)
{
  m_hostDataContext = value;
  if (m_instance != nullptr)
  {
    m_instance->bind(value);
    m_instance->flush();
  }
}

void TextStyleGlobal::relinkHostDataContext()
{
  if (m_instance != nullptr)
  {
    m_instance->relink();
  }
}

void TextStyleGlobal::updateHostedDataBinds()
{
  // The host's binds just wrote the values this style reads (its exposed
  // inputs, the host's own data): apply them in this same frame.
  if (m_instance != nullptr)
  {
    m_instance->flush();
  }
}

bool TextStyleGlobal::advanceComponent(float elapsedSeconds, AdvanceFlags flags)
{
  // A shared instance never changes.
  if (m_instance == nullptr)
  {
    return false;
  }
  return m_instance->advance(elapsedSeconds, flags);
}

AdvancingComponent::QuietState TextStyleGlobal::quietState()
{
  if (m_instance == nullptr)
  {
    return QuietState::quiet;
  }
  // A bound view model instance can change at any time without waking this
  // row.
  if (m_boundViewModelInstance != nullptr)
  {
    return QuietState::never;
  }
  return m_instance->quietState();
}

bool TextStyleGlobal::listensToRuns() const
{
  return m_instance != nullptr && m_info.listensToRuns;
}

rcp<ViewModelInstance> TextStyleGlobal::styleViewModelInstance() const
{
  return m_instance != nullptr ? m_instance->viewModelInstance() : nullptr;
}

StateMachineInstance* TextStyleGlobal::runStateMachine() const
{
  return listensToRuns() ? m_instance->stateMachine() : nullptr;
}

void TextStyleGlobal::listItem(const rcp<ViewModelInstance>& value)
{
  m_listItem = value;
  if (m_instance != nullptr)
  {
    m_instance->listItem(value);
  }
}

ArtboardInstance* TextStyleGlobal::artboardInstance() const
{
  if (m_instance != nullptr)
  {
    return m_instance->artboardInstance();
  }
  return m_shared ? m_shared.get()->artboardInstance() : nullptr;
}

TextStylePaint* TextStyleGlobal::resolvedPaint()
{
  if (m_paint != nullptr)
  {
    return m_paint;
  }
  // The referenced source artboard on the file's own (non-instance) host.
  Artboard* source = m_referencedArtboard;
#ifdef WITH_RIVE_EDITOR
  // Coop-delivered handles are never resolved by the BackboardImporter; the
  // editor's resolver maps their artboardId file-wide. (A runtime artboardId
  // is a file artboard index, not an object index, so this is editor-only.)
  if (source == nullptr)
  {
    Artboard* host = artboard();
    Core* object = host != nullptr ? host->resolve(artboardId()) : nullptr;
    if (object != nullptr && object->is<Artboard>())
    {
      source = static_cast<Artboard*>(object);
    }
  }
#endif
  return firstTextStylePaint(source);
}
