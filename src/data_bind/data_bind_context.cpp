#include "rive/data_bind_flags.hpp"
#include "rive/data_bind/data_bind_context.hpp"
#include "rive/data_bind/context/context_value_number.hpp"
#include "rive/data_bind/context/context_value_string.hpp"
#include "rive/data_bind/context/context_value_enum.hpp"
#include "rive/data_bind/context/context_value_list.hpp"
#include "rive/data_bind/context/context_value_color.hpp"
#include "rive/data_bind/converters/data_converter_group.hpp"
#include "rive/artboard.hpp"
#include "rive/generated/core_registry.hpp"
#ifdef WITH_RIVE_EDITOR
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#endif
#include <iostream>

using namespace rive;

void DataBindContext::decodeSourcePathIds(Span<const uint8_t> value)
{
  BinaryReader reader(value);
  while (!reader.reachedEnd())
  {
    auto val = reader.readVarUintAs<uint32_t>();
    m_SourcePathIdsBuffer.push_back(val);
  }
}

void DataBindContext::copySourcePathIds(const DataBindContextBase& object)
{
  m_SourcePathIdsBuffer = object.as<DataBindContext>()->m_SourcePathIdsBuffer;
  m_isPathResolved = object.as<DataBindContext>()->m_isPathResolved;
#ifdef WITH_RIVE_EDITOR
  m_editorSourcePath = object.as<DataBindContext>()->m_editorSourcePath;
  m_hasEditorSourcePath = object.as<DataBindContext>()->m_hasEditorSourcePath;
#endif
}

#ifdef WITH_RIVE_EDITOR
void DataBindContext::decodeEditorSourcePathIds(BinaryReader& reader)
{
  m_hasEditorSourcePath = false;
  m_editorSourcePath.clear();
  // Coop lists contain a count followed by full (client, object) ID pairs.
  const auto count = reader.readVarUintAs<uint32_t>();
  for (uint32_t i = 0; i < count && !reader.reachedEnd() && !reader.hasError();
       ++i)
    m_editorSourcePath.push_back(CoreIdType::deserialize(reader));
  if (reader.hasError() || m_editorSourcePath.size() != count)
    m_editorSourcePath.clear();
  else
    m_hasEditorSourcePath = true;
}
#endif

void DataBindContext::resolvePath()
{
  if (!isNameBased() || m_isPathResolved)
  {
    return;
  }
  m_isPathResolved = true;
  if (m_SourcePathIdsBuffer.size() > 0)
  {
    auto pathId = m_SourcePathIdsBuffer[0];
    if (m_file)
    {
      auto dataResolver = m_file->dataResolver();
      if (dataResolver)
      {
        auto resolvedPath = dataResolver->resolvePath(pathId);
        if (!resolvedPath.empty())
        {
          m_SourcePathIdsBuffer = resolvedPath;
        }
      }
    }
  }
}

void DataBindContext::bindFromContext(DataContext* dataContext)
{
  if (dataContext != nullptr)
  {
    resolvePath();
    auto vmSource =
#ifdef WITH_RIVE_EDITOR
        m_hasEditorSourcePath
            ? dataContext->getEditorViewModelProperty(m_editorSourcePath)
        :
#endif
        isNameBased() && file()
            ? dataContext->getRelativeViewModelProperty(
                  m_SourcePathIdsBuffer,
                  file()->dataResolver())
            : dataContext->getViewModelProperty(m_SourcePathIdsBuffer);
    if (m_Source == nullptr || vmSource != m_Source.get())
    {
      if (vmSource != nullptr)
      {
        clearSource();
        source(ref_rcp(vmSource));
        bind();
      }
      else
      {
        unbind();
      }
    }
    else
    {
      // Source unchanged — re-sync both supported directions in favor
      // order (a reconcile, not a one-sided change). See bind().
      addDirt(reconcileDirt(), true);
    }
    if (m_dataConverter != nullptr)
    {
      m_dataConverter->bindFromContext(dataContext, this);
    }
  }
}
