#include "rive/text/text_style_on_demand.hpp"
#include "rive/artboard.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/file.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_style_instance.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"

using namespace rive;

TextStyleOnDemand::TextStyleOnDemand(
    const Artboard* source,
    rcp<ViewModelInstance> boundViewModelInstance,
    File* file,
    rcp<const File> crossFile,
    Factory* factory,
    rcp<DataContext> parent,
    Text* text,
    rcp<ViewModelInstance> item,
    rcp<ViewModelInstance> valuesSource)
    : m_source(source)
    , m_crossFile(std::move(crossFile))
    , m_boundViewModelInstance(std::move(boundViewModelInstance))
    , m_item(std::move(item))
{
  // View model ids are per file: the style's view model lives in the file
  // that owns the style artboard.
  const File* viewModelFile = m_crossFile != nullptr ? m_crossFile.get() : file;
  TextStyleShareInfo info;
  if (m_crossFile == nullptr && file != nullptr)
  {
    info = file->textStyleShares().info(source);
    if (m_boundViewModelInstance == nullptr && info.shareable)
    {
      m_shared = file->textStyleShares().acquire(source, factory, text);
      if (m_shared)
      {
        m_paint = m_shared.get()->paint();
        return;
      }
    }
  }
  else
  {
    info = TextStyleShareCache::analyze(source, viewModelFile);
  }
  m_info = info;

  rcp<ViewModelInstance> viewModelInstance = m_boundViewModelInstance;
  if (viewModelInstance == nullptr && viewModelFile != nullptr)
  {
    auto viewModel = viewModelFile->viewModel(source->viewModelId());
    if (viewModel != nullptr)
    {
      viewModelInstance = viewModelFile->createDefaultViewModelInstance(
          viewModel);
      if (viewModelInstance != nullptr && m_crossFile == nullptr &&
          file != nullptr)
      {
        file->completeViewModelProperties(viewModelInstance.get());
      }
    }
  }
  // A style listening to its runs listens to every list run drawn with
  // this style, as one.
  m_instance = std::make_unique<TextStyleInstance>(
      source,
      factory,
      viewModelInstance,
      info.readsOutsideOwnViewModel,
      text,
      info.listensToRuns ? text : nullptr,
      this);
  m_paint = m_instance->paint();
  m_instance->copyValuesFrom(std::move(valuesSource));
  m_instance->bind(parent);
  // Shape against the bound values from the first frame.
  auto instance = m_instance->artboardInstance();
  instance->updateDataBinds();
  instance->updatePass(false);
}

TextStyleOnDemand::~TextStyleOnDemand()
{
  // The instance's data binds reference the view model instance.
  m_paint = nullptr;
  m_instance = nullptr;
  m_shared.release();
  m_boundViewModelInstance = nullptr;
  m_item = nullptr;
}

bool TextStyleOnDemand::listensToRuns() const
{
  return m_instance != nullptr && m_info.listensToRuns;
}

StateMachineInstance* TextStyleOnDemand::runStateMachine() const
{
  return listensToRuns() ? m_instance->stateMachine() : nullptr;
}

void TextStyleOnDemand::listItem(const rcp<ViewModelInstance>& value)
{
  if (m_instance != nullptr)
  {
    m_instance->listItem(value);
  }
}

void TextStyleOnDemand::hostDataContext(rcp<DataContext> parent)
{
  if (m_instance != nullptr)
  {
    m_instance->bind(parent);
    m_instance->flush();
  }
}

void TextStyleOnDemand::relinkHostDataContext()
{
  if (m_instance != nullptr)
  {
    m_instance->relink();
  }
}

bool TextStyleOnDemand::updateHostedDataBinds()
{
  if (m_instance == nullptr)
  {
    return false;
  }
  // The values source (a handle's, whose binds just ran) may have new
  // exposed inputs.
  m_instance->syncInputs();
  return m_instance->flush();
}

bool TextStyleOnDemand::advance(
    float elapsedSeconds,
    AdvanceFlags flags,
    bool& changed)
{
  if (m_instance == nullptr)
  {
    return false;
  }
  bool wasDirty = m_instance->artboardInstance()->hasDirt(
      ComponentDirt::Components);
  bool keepGoing = m_instance->advance(elapsedSeconds, flags);
  if (wasDirty)
  {
    changed = true;
  }
  return keepGoing;
}

AdvancingComponent::QuietState TextStyleOnDemand::quietState() const
{
  if (m_instance == nullptr)
  {
    return AdvancingComponent::QuietState::quiet;
  }
  // An item-supplied view model the host can change at any time.
  if (m_boundViewModelInstance != nullptr)
  {
    return AdvancingComponent::QuietState::never;
  }
  return m_instance->quietState();
}
