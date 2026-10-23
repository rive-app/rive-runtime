#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_symbol_list_index.hpp"
#include "rive/file.hpp"
#include "rive/backboard.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/viewmodel/viewmodel_property_viewmodel.hpp"
#include "rive/core_context.hpp"
#include "rive/refcnt.hpp"
#include "rive/artboard.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind_flags.hpp"

using namespace rive;

ViewModelInstance::~ViewModelInstance()
{
  for (auto dataBind : m_valueDataBinds)
  {
    delete dataBind;
  }
  // Holders such as ViewModelInstanceValueRuntime can keep a value past this
  // instance, so nothing they reach may still point at it.
  for (auto& value : m_PropertyValues)
  {
    if (value->viewModelInstance() == this)
    {
      value->viewModelInstance(nullptr);
    }
    if (value->is<ViewModelInstanceViewModel>())
    {
      auto vmInstanceViewModel = value->as<ViewModelInstanceViewModel>();
      if (vmInstanceViewModel->referenceViewModelInstance())
      {
        vmInstanceViewModel->referenceViewModelInstance()->removeParent(this);
      }
      if (vmInstanceViewModel->parentViewModelInstance() == this)
      {
        vmInstanceViewModel->parentViewModelInstance(nullptr);
      }
    }
    else if (value->is<ViewModelInstanceList>())
    {
      auto list = value->as<ViewModelInstanceList>();
      if (list->parentViewModelInstance() == this)
      {
        list->detachParent();
      }
    }
  }
  m_PropertyValues.clear();
  if (m_ViewModel != nullptr)
  {
    m_ViewModel->unref();
  }
}

void ViewModelInstance::addValue(ViewModelInstanceValue* value)
{
  if (value == nullptr)
  {
    return;
  }
  // Check if already added (can happen when both import() and onAddedDirty()
  // add the same value).
  for (const auto& existing : m_PropertyValues)
  {
    if (existing.get() == value)
    {
      return;
    }
  }
  appendValue(value);
}

void ViewModelInstance::appendValue(ViewModelInstanceValue* value)
{
  // Registers the value's symbol too.
  value->viewModelInstance(this);
#ifdef WITH_RIVE_EDITOR
  // Hydrated values keep their initial reference in the arena. The runtime
  // importer, in contrast, transfers that initial reference to its VMI.
  if (value->editorArena() != nullptr)
  {
    value->ref();
  }
#endif
  m_PropertyValues.push_back(rcp<ViewModelInstanceValue>(value));
}

bool ViewModelInstance::removeValue(uint32_t propertyId)
{
  for (auto it = m_PropertyValues.begin(); it != m_PropertyValues.end(); ++it)
  {
    auto value = *it;
    if (value->viewModelPropertyId() != propertyId)
    {
      continue;
    }
    // Binds aimed at this value, ours and the clones containers hold,
    // would dangle once it is freed.
    for (auto dataBind : LazyVector<DataBind*>(m_valueDataBinds))
    {
      if (dataBind->target() == value.get())
      {
        m_valueDataBinds.eraseAll(dataBind);
        delete dataBind;
      }
    }
    for (auto* dependent : std::vector<DataBindContainer*>(m_dependents.view()))
    {
      dependent->dropInstanceValueBindsTargeting(value.get());
    }
    // Mirror the destructor cleanup for nested view model references.
    if (value->is<ViewModelInstanceViewModel>())
    {
      auto vmInstanceViewModel = value->as<ViewModelInstanceViewModel>();
      if (vmInstanceViewModel->referenceViewModelInstance())
      {
        vmInstanceViewModel->referenceViewModelInstance()->removeParent(this);
      }
    }
    // Drop any symbol-table entry pointing at this value.
    for (auto sit = m_propertySymbols.begin(); sit != m_propertySymbols.end();)
    {
      if (sit->second == value.get())
      {
        sit = m_propertySymbols.erase(sit);
      }
      else
      {
        ++sit;
      }
    }
    m_PropertyValues.erase(it); // rcp releases the value
    return true;
  }
  return false;
}

ViewModelInstanceValue* ViewModelInstance::propertyValue(const uint32_t id)
{
  for (auto value : m_PropertyValues)
  {
    if (value->viewModelPropertyId() == id)
    {
      return value.get();
    }
  }
  return nullptr;
}

bool ViewModelInstance::replaceViewModelByName(
    const std::string& name,
    rcp<ViewModelInstance> value)
{
  auto viewModelProperty = viewModel()->property(name);
  if (viewModelProperty != nullptr)
  {
    for (auto propertyValue : m_PropertyValues)
    {
      if (propertyValue->viewModelProperty() == viewModelProperty)
      {
        if (value->viewModelId() ==
            viewModelProperty->as<ViewModelPropertyViewModel>()
                ->viewModelReferenceId())
        {
          // Share the by-property implementation so both entry points
          // behave identically — same invalidation sequence, same
          // same-value guard. Callers must not see different churn
          // depending on which API they reached for.
          return replaceViewModelByProperty(
              propertyValue->as<ViewModelInstanceViewModel>(),
              value);
        }
        break;
      }
    }
  }
  return false;
}

bool ViewModelInstance::replaceViewModelByProperty(
    ViewModelInstanceViewModel* property,
    rcp<ViewModelInstance> value)
{
  for (auto& propertyValue : m_PropertyValues)
  {
    if (propertyValue.get() == property)
    {
      auto previousViewModelInstance = propertyValue
                                           ->as<ViewModelInstanceViewModel>()
                                           ->referenceViewModelInstance();
      // Assigning the instance the property already holds is a no-op,
      // but the work below is not: it re-parents, marks Bindings dirt,
      // fires the WITH_RIVE_TOOLS changed callback, and relinks every
      // dependent. The data-bind apply path reaches here on every
      // advance (DataBindContextValueViewModel::apply ->
      // updateViewModel) without comparing first, so an unchanged
      // reference would churn all of that every frame. The invalidation
      // below exists to drop caches pointing at a *previous* instance;
      // with no swap there are none. Still reports success: the property
      // was found and holds the requested value.
      if (previousViewModelInstance == value)
      {
        return true;
      }
      propertyValue->as<ViewModelInstanceViewModel>()
          ->referenceViewModelInstance(value);
      // Invalidate value-level dependents (e.g. scripted property
      // wrappers) so cached references to the previous instance are
      // dropped. Multiple dependents can share this property. Snapshot
      // because relinkDataBind can mutate the dependents list.
      auto dependentsSnapshot = propertyValue->dependents();
      for (auto& dependent : dependentsSnapshot)
      {
        dependent->relinkDataBind();
      }
      rebindDependents();
      if (previousViewModelInstance)
      {
        previousViewModelInstance->rebindProperties();
      }
      return true;
    }
  }
  return false;
}

void ViewModelInstance::propertyValue(
    const SymbolType symbolType,
    ViewModelInstanceValue* value)
{
  if (symbolType != SymbolType::none)
  {
    for (auto& entry : m_propertySymbols)
    {
      if (entry.first == symbolType)
      {
        entry.second = value;
        return;
      }
    }
    m_propertySymbols.emplace_back(symbolType, value);
  }
}

ViewModelInstanceValue* ViewModelInstance::propertyValue(
    const SymbolType symbolType)
{

  for (auto& entry : m_propertySymbols)
  {
    if (entry.first == symbolType)
    {
      return entry.second;
    }
  }
  return nullptr;
}

ViewModelInstanceValue* ViewModelInstance::propertyValue(
    const std::string& name)
{
  for (auto value : m_PropertyValues)
  {
    if (value->viewModelProperty() &&
        value->viewModelProperty()->name() == name)
    {
      return value.get();
    }
  }
  return nullptr;
}

void ViewModelInstance::viewModel(ViewModel* value)
{
  if (value != nullptr)
  {
    value->ref();
  }
  if (m_ViewModel != nullptr)
  {
    m_ViewModel->unref();
  }
  m_ViewModel = value;
}

ViewModel* ViewModelInstance::viewModel() const { return m_ViewModel; }

void ViewModelInstance::onComponentDirty(Component* component) {}

void ViewModelInstance::setAsRoot(rcp<ViewModelInstance> instance)
{
  setRoot(instance);
}

void ViewModelInstance::setRoot(rcp<ViewModelInstance> value)
{
  for (auto propertyValue : m_PropertyValues)
  {
    propertyValue->setRoot(value);
  }
}

const std::vector<rcp<ViewModelInstanceValue>>& ViewModelInstance::
    propertyValues()
{
  return m_PropertyValues;
}

Core* ViewModelInstance::clone() const
{
  auto cloned = new ViewModelInstance();
  cloned->copy(*this);

  // If we have an artboard, it means we will be in the artboard's object
  // list which will clone our property values for us
  if (artboard() == nullptr)
  {
    cloned->reserveValues(m_PropertyValues.size());
    for (const auto& propertyValue : m_PropertyValues)
    {
      auto clonedValue = propertyValue->clone()->as<ViewModelInstanceValue>();
#ifdef WITH_RIVE_EDITOR
      // Preserve shared property metadata for native REV model clones.
      clonedValue->viewModelProperty(propertyValue->viewModelProperty());
#endif
      cloned->appendValue(clonedValue);
    }
    for (auto dataBind : m_valueDataBinds)
    {
      for (size_t i = 0; i < m_PropertyValues.size(); i++)
      {
        if (m_PropertyValues[i].get() == dataBind->target())
        {
          cloned->addValueDataBind(
              dataBind->cloneWithTarget(cloned->m_PropertyValues[i].get()));
          break;
        }
      }
    }
  }
  cloned->viewModel(viewModel());
  return cloned;
}

void ViewModelInstance::addValueDataBind(DataBind* dataBind)
{
  // The authored value is only a default, so the source wins the reconcile.
  if (dataBind->toSource() && dataBind->toTarget())
  {
    dataBind->flags(
        dataBind->flags() |
        static_cast<uint32_t>(DataBindFlags::SourceToTargetRunsFirst));
  }
  m_valueDataBinds.push_back(dataBind);
}

StatusCode ViewModelInstance::import(ImportStack& importStack)
{
  auto backboardImporter = importStack.latest<BackboardImporter>(
      Backboard::typeKey);
  if (backboardImporter == nullptr)
  {
    return StatusCode::MissingObject;
  }

  backboardImporter->addViewModelInstance(this);

  auto artboardImporter = importStack.latest<ArtboardImporter>(
      ArtboardBase::typeKey);
  if (artboardImporter != nullptr)
  {
    return Super::import(importStack);
  }

  // Only add ViewModelInstances at the File level if they are not
  // in the Component hierarchy.
  auto file = backboardImporter->file();
  if (file != nullptr)
  {
    file->addFileViewModelInstance(this);
  }

  return StatusCode::Ok;
}

ViewModelInstanceValue* ViewModelInstance::propertyFromPath(
    std::vector<uint32_t>* path,
    size_t index)
{
  if (index < path->size())
  {
    auto propertyId = (*path)[index];
    auto property = propertyValue(propertyId);
    if (property != nullptr)
    {
      if (index == path->size() - 1)
      {
        return property;
      }
      if (property->is<ViewModelInstanceViewModel>())
      {
        auto propertyViewModel = property->as<ViewModelInstanceViewModel>();
        auto viewModelInstance = propertyViewModel
                                     ->referenceViewModelInstance();
        return viewModelInstance->propertyFromPath(path, index + 1);
      }
    }
  }
  return nullptr;
}

void ViewModelInstance::addParent(ViewModelInstance* parent)
{
  if (parent == nullptr || parent == m_parent)
  {
    return;
  }
  if (m_parent == nullptr)
  {
    m_parent = parent;
    return;
  }
  m_moreParents.pushUnique(parent);
}

void ViewModelInstance::removeParent(ViewModelInstance* parent)
{
  m_moreParents.eraseAll(parent);
  if (m_parent == parent)
  {
    auto& more = m_moreParents.view();
    m_parent = more.empty() ? nullptr : more.back();
    m_moreParents.eraseAll(m_parent);
  }
}

void ViewModelInstance::addDependent(DataBindContainer* dependent)
{
  if (!dependent)
  {
    return;
  }
  m_dependents.pushUnique(dependent);
}

void ViewModelInstance::removeDependent(DataBindContainer* dependent)
{
  m_dependents.eraseAll(dependent);
}

void ViewModelInstance::rebindProperties()
{
  for (auto& property : m_PropertyValues)
  {
    auto dependents = property->dependents();
    for (auto& dependent : dependents)
    {
      dependent->relinkDataBind();
    }
    if (property->is<ViewModelInstanceViewModel>())
    {
      auto viewModelInstance = property->as<ViewModelInstanceViewModel>()
                                   ->referenceViewModelInstance();
      if (viewModelInstance)
      {
        viewModelInstance->rebindProperties();
      }
    }
  }
}

void ViewModelInstance::rebindDependents()
{
  for (auto& dependent : m_dependents)
  {
    dependent->relinkDataContext();
  }
  if (m_parent != nullptr)
  {
    m_parent->rebindDependents();
  }
  for (auto* parent : m_moreParents)
  {
    parent->rebindDependents();
  }
}

#ifdef WITH_RIVE_EDITOR
void ViewModelInstance::editorPrepareForDestruction()
{
  for (auto* bind : m_valueDataBinds)
  {
    if (bind->editorArena() == nullptr)
      delete bind;
  }
  m_valueDataBinds.clear();
  Component::editorPrepareForDestruction();
}

#endif

#ifdef WITH_RIVE_EDITOR
void ViewModelInstance::detachValueForEditor(ViewModelInstanceValue* value)
{
  // Removing a coop value must relinquish its container reference now, not
  // keep a removed allocation alive until this entire file is retired.
  for (auto* bind : LazyVector<DataBind*>(m_valueDataBinds))
  {
    if (bind->target() == value)
    {
      m_valueDataBinds.eraseAll(bind);
      if (bind->editorArena() == nullptr)
        delete bind;
      else
        bind->editorPrepareForDestruction();
    }
  }
  for (auto* dependent : std::vector<DataBindContainer*>(m_dependents.view()))
    dependent->dropInstanceValueBindsTargeting(value);
  m_propertySymbols.erase(
      std::remove_if(
          m_propertySymbols.begin(),
          m_propertySymbols.end(),
          [value](const auto& symbol) { return symbol.second == value; }),
      m_propertySymbols.end());
  m_PropertyValues.erase(
      std::remove_if(
          m_PropertyValues.begin(),
          m_PropertyValues.end(),
          [value](const auto& entry) { return entry.get() == value; }),
      m_PropertyValues.end());
}
#endif
