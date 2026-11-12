#ifndef _RIVE_VIEW_MODEL_HPP_
#define _RIVE_VIEW_MODEL_HPP_
#include "rive/generated/viewmodel/viewmodel_base.hpp"
#include "rive/viewmodel/viewmodel_property.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/symbol_type.hpp"
#include "rive/refcnt.hpp"
#include <atomic>
#include <stdio.h>
namespace rive
{
class File;
struct DeferredViewModelInstanceBody;
class ViewModel : public ViewModelBase, public RefCnt<ViewModel>
{
private:
  std::vector<ViewModelProperty*> m_Properties;
  std::vector<ViewModelInstance*> m_Instances;
  File* m_file = nullptr;
  /// The values the import deferred for each instance, by its index in
  /// m_Instances (only file level instances, which the import lists first),
  /// null once decoded. Its size is set during the import; its entries are
  /// read and written under deferredDecodeMutex() only.
  struct DeferredInstance
  {
    ViewModelInstance* instance;
    DeferredViewModelInstanceBody* body;
  };
  std::vector<DeferredInstance> m_deferredBodies;
  /// How many of m_deferredBodies are still pending: none lets every
  /// handout skip the lock.
  std::atomic<uint32_t> m_pendingBodies{0};
  /// Decodes the values of the instance at [index] if the import deferred
  /// them.
  void ensureDecoded(size_t index)
  {
    if (m_pendingBodies.load(std::memory_order_acquire) != 0 &&
        index < m_deferredBodies.size())
    {
      decodeInstanceBody(index);
    }
  }
  void decodeInstanceBody(size_t index);

public:
#ifdef WITH_RIVE_EDITOR
  void editorPrepareForDestruction() override;
  void editorRelease() override { unref(); }
#endif
  ~ViewModel();
  void addProperty(ViewModelProperty* property);
  ViewModelProperty* property(const std::string& name);
  ViewModelProperty* property(SymbolType symbolType);
  ViewModelProperty* property(size_t index);
  void addInstance(ViewModelInstance* value);
  ViewModelInstance* instance(size_t index);
  ViewModelInstance* instance(const std::string& name);
  rcp<ViewModelInstance> createInstance();
  rcp<ViewModelInstance> createFromInstance(const std::string& instanceName);
  void file(File* value) { m_file = value; };
#ifdef WITH_RIVE_TOOLS
  File* file() { return m_file; };
#endif
  ViewModelInstance* defaultInstance();
  size_t instanceCount() const;
  const std::vector<ViewModelProperty*>& properties() const
  {
    return m_Properties;
  }
  std::vector<ViewModelInstance*> instances();
  /// The name of the instance at [index] (empty when out of range), which
  /// came with its record: reading it decodes nothing.
  const std::string& instanceName(size_t index) const;
  /// Whether the import deferred the values of the instance at [index]
  /// and they haven't been decoded yet (see FileImportOptions).
  bool hasDeferredInstanceBody(size_t index);

private:
  /// The import deferred the values of the instance at [index]
  /// (File::deferViewModelInstanceBody).
  void deferInstanceBody(size_t index, DeferredViewModelInstanceBody* body);
  /// Frees the values still deferred, which nothing can decode once the file
  /// is gone.
  void discardDeferredInstanceBodies();
  friend class File;
};
} // namespace rive

#endif