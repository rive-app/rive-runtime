#include "rive/data_bind/data_bind_path.hpp"
#include "rive/importers/backboard_importer.hpp"

using namespace rive;

void DataBindPath::decodePath(Span<const uint8_t> value)
{
    BinaryReader reader(value);
    while (!reader.reachedEnd())
    {
        auto val = reader.readVarUintAs<uint32_t>();
        m_pathBuffer.push_back(val);
    }
}

void DataBindPath::copyPath(const DataBindPathBase& object)
{
    m_pathBuffer = object.as<DataBindPath>()->m_pathBuffer;
    m_resolved = object.as<DataBindPath>()->m_resolved;
}

StatusCode DataBindPath::import(ImportStack& importStack)
{
    auto backboardImporter =
        importStack.latest<BackboardImporter>(Backboard::typeKey);
    if (backboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    auto file = backboardImporter->file();
    m_file = file;

    return Super::import(importStack);
}

const std::vector<uint32_t>& DataBindPath::resolvedPath()
{
    if (!m_resolved)
    {
        if (!m_file)
        {
            return m_pathBuffer;
        }
        auto dataResolver = m_file->dataResolver();
        if (dataResolver && m_pathBuffer.size() == 1)
        {
            auto pathId = m_pathBuffer[0];
            m_pathBuffer = dataResolver->resolvePath(pathId);
        }
        m_resolved = true;
    }
    return m_pathBuffer;
}

void DataBindPath::file(File* file) { m_file = file; }