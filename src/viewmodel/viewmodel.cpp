#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_property.hpp"
#include "rive/viewmodel/deferred_viewmodel_instance_body.hpp"
#include "rive/backboard.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/deferred_decode_lock.hpp"
#include "rive/file.hpp"
#include "rive/importers/backboard_importer.hpp"

#include <cassert>

using namespace rive;

namespace
{
// Set while this thread decodes an instance's values. Decoding one never
// needs another decoded: a value names the instance it refers to by index,
// which only a copy follows.
thread_local bool t_decodingInstance = false;
} // namespace

ViewModel::~ViewModel()
{
  discardDeferredInstanceBodies();
  for (auto& property : m_Properties)
  {
    delete property;
  }
}

void ViewModel::deferInstanceBody(
    size_t index,
    DeferredViewModelInstanceBody* body)
{
  // During the import, before anything else can see the view model.
  if (m_deferredBodies.size() <= index)
  {
    m_deferredBodies.resize(index + 1, {nullptr, nullptr});
  }
  m_deferredBodies[index] = {m_Instances[index], body};
#ifdef TESTING
  m_Instances[index]->m_testingBodyDeferred = true;
#endif
  m_pendingBodies.fetch_add(1, std::memory_order_relaxed);
}

void ViewModel::decodeInstanceBody(size_t index)
{
  assert(!t_decodingInstance);
  std::lock_guard<std::recursive_mutex> lock(deferredDecodeMutex());
  // Another thread may have decoded it while this one waited for the lock.
  if (index >= m_deferredBodies.size() ||
      m_deferredBodies[index].body == nullptr)
  {
    return;
  }
  DeferredInstance deferred = m_deferredBodies[index];
  t_decodingInstance = true;
#ifdef TESTING
  deferred.instance->m_testingBodyDeferred = false;
#endif
  // The file outlives its pending bodies (discardDeferredInstanceBodies).
  if (!m_file->decodeViewModelInstanceBody(deferred.instance, *deferred.body))
  {
    // Only a malformed file fails here (the import would have failed it
    // whole); the instance keeps the values it read.
    fprintf(
        stderr,
        "ViewModel: couldn't decode the deferred values of instance %s.\n",
        deferred.instance->name().c_str());
    m_file->m_deferredViewModelInstanceStats.decodeFailures++;
  }
  t_decodingInstance = false;
#ifdef TESTING
  File::deferredViewModelInstanceDecodeCount++;
  // Its binds have their converters before it's published: another thread
  // may copy it without the lock as soon as it is. (Inside the import, the
  // import resolves them, and nothing else sees the file until it's done.)
  if (!m_file->m_importing)
  {
    for (DataBind* bind : deferred.instance->valueDataBinds())
    {
      assert(
          (size_t)bind->converterId() >= m_file->m_DataConverters.size() ||
          bind->converter() != nullptr);
    }
  }
#endif
  m_deferredBodies[index].body = nullptr;
  delete deferred.body;
  m_pendingBodies.fetch_sub(1, std::memory_order_release);
}

bool ViewModel::hasDeferredInstanceBody(size_t index)
{
  if (m_pendingBodies.load(std::memory_order_acquire) == 0)
  {
    return false;
  }
  std::lock_guard<std::recursive_mutex> lock(deferredDecodeMutex());
  return index < m_deferredBodies.size() &&
         m_deferredBodies[index].body != nullptr;
}

void ViewModel::discardDeferredInstanceBodies()
{
  if (m_pendingBodies.load(std::memory_order_acquire) == 0)
  {
    return;
  }
  std::lock_guard<std::recursive_mutex> lock(deferredDecodeMutex());
  for (auto& deferred : m_deferredBodies)
  {
    delete deferred.body;
    deferred.body = nullptr;
  }
  m_pendingBodies.store(0, std::memory_order_release);
}

void ViewModel::addProperty(ViewModelProperty* property)
{
  m_Properties.push_back(property);
}

ViewModelProperty* ViewModel::property(size_t index)
{
  if (index < m_Properties.size())
  {
    return m_Properties[index];
  }
  return nullptr;
}

ViewModelProperty* ViewModel::property(const std::string& propName)
{
  for (auto property : m_Properties)
  {
    if (property->name() == propName)
    {
      return property;
    }
  }
  return nullptr;
}

ViewModelProperty* ViewModel::property(const SymbolType symbolType)
{
  for (auto property : m_Properties)
  {
    if (property->symbolTypeValue() == static_cast<int>(symbolType))
    {
      return property;
    }
  }
  return nullptr;
}

void ViewModel::addInstance(ViewModelInstance* value)
{
  m_Instances.push_back(value);
  value->viewModel(this);
}

// Every instance handed out is decoded first: whatever reads its values,
// copies it or binds it goes through one of these.
ViewModelInstance* ViewModel::defaultInstance()
{
  if (m_Instances.size() > 0)
  {
    ensureDecoded(0);
    return m_Instances[0];
  }
  return nullptr;
}

ViewModelInstance* ViewModel::instance(size_t index)
{
  if (index < m_Instances.size())
  {
    ensureDecoded(index);
    return m_Instances[index];
  }
  return nullptr;
}

ViewModelInstance* ViewModel::instance(const std::string& name)
{
  // A deferred instance's name came with its record.
  for (size_t index = 0; index < m_Instances.size(); index++)
  {
    if (m_Instances[index]->name() == name)
    {
      ensureDecoded(index);
      return m_Instances[index];
    }
  }
  return nullptr;
}

const std::string& ViewModel::instanceName(size_t index) const
{
  static const std::string empty;
  return index < m_Instances.size() ? m_Instances[index]->name() : empty;
}

std::vector<ViewModelInstance*> ViewModel::instances()
{
  if (m_pendingBodies.load(std::memory_order_acquire) != 0)
  {
    for (size_t index = 0; index < m_Instances.size(); index++)
    {
      ensureDecoded(index);
    }
  }
  return m_Instances;
}

size_t ViewModel::instanceCount() const { return m_Instances.size(); }

rcp<ViewModelInstance> ViewModel::createInstance()
{
  if (m_file)
  {
    return m_file->createViewModelInstance(this);
  }
  return nullptr;
}

rcp<ViewModelInstance> ViewModel::createFromInstance(
    const std::string& instanceName)
{
  if (m_file)
  {
    return m_file->createViewModelInstance(this->name(), instanceName);
  }
  return nullptr;
}
#ifdef WITH_RIVE_EDITOR
void ViewModel::editorPrepareForDestruction()
{
  // Definitions can be externally retained after the arena retires.
  // Their instance lookup table must not expose freed arena entries.
  m_Instances.erase(
      std::remove_if(
          m_Instances.begin(),
          m_Instances.end(),
          [](auto* instance) { return instance->editorArena() != nullptr; }),
      m_Instances.end());
  m_Properties.erase(
      std::remove_if(
          m_Properties.begin(),
          m_Properties.end(),
          [](auto* c) { return c->editorArena() != nullptr; }),
      m_Properties.end());
}

#endif
