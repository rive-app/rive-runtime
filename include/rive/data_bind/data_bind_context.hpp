#ifndef _RIVE_DATA_BIND_CONTEXT_HPP_
#define _RIVE_DATA_BIND_CONTEXT_HPP_
#include "rive/generated/data_bind/data_bind_context_base.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/refcnt.hpp"
#include "rive/sidecar.hpp"
#include <stdio.h>
namespace rive
{
class DataBindContext : public DataBindContextBase
{
protected:
  // Shared with the bind this one was cloned from: instances of an artboard
  // bind to the same paths, and only a name based path that resolves
  // differently needs one of its own.
  SharedSidecar<std::vector<uint32_t>> m_sourcePathIds;
#ifdef WITH_RIVE_EDITOR
  std::vector<Id> m_editorSourcePath;
  bool m_hasEditorSourcePath = false;
#endif

public:
  void decodeSourcePathIds(Span<const uint8_t> value) override;
  void copySourcePathIds(const DataBindContextBase& object) override;
  void bindFromContext(DataContext* dataContext);
#ifdef WITH_RIVE_EDITOR
  // Coop paths retain (client, object) IDs, unlike RIV's flat indices.
  void decodeEditorSourcePathIds(BinaryReader& reader);
#endif
  const std::vector<uint32_t>& sourcePathIds() const
  {
    static const std::vector<uint32_t> empty;
    auto ids = m_sourcePathIds.get();
    return ids != nullptr ? *ids : empty;
  }
  // The source path's length. A path the editor hydrated is kept apart and
  // leaves sourcePathIds empty.
  size_t sourcePathLength() const
  {
#ifdef WITH_RIVE_EDITOR
    if (m_hasEditorSourcePath)
    {
      return m_editorSourcePath.size();
    }
#endif
    return sourcePathIds().size();
  }

private:
  void resolvePath();
  bool m_isPathResolved = false;
};
} // namespace rive

#endif
