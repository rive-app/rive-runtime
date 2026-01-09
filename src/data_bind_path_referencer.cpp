#include "rive/data_bind_path_referencer.hpp"
#include "rive/importers/data_bind_path_importer.hpp"
#include "rive/file.hpp"

using namespace rive;

DataBindPathReferencer::~DataBindPathReferencer()
{
    if (m_dataBindPath)
    {
        delete m_dataBindPath;
    }
}

void DataBindPathReferencer::copyDataBindPath(DataBindPath* dataBindPath)
{
    if (dataBindPath)
    {
        m_dataBindPath = dataBindPath->clone()->as<DataBindPath>();
        m_dataBindPath->file(dataBindPath->file());
    }
}

void DataBindPathReferencer::importDataBindPath(ImportStack& importStack)
{
    auto dataBindPathImporter =
        importStack.latest<DataBindPathImporter>(DataBindPathBase::typeKey);
    if (dataBindPathImporter)
    {
        auto dataBindPath = dataBindPathImporter->claim();
        if (dataBindPath != nullptr)
        {
            // If a DataBindPath is claimed, the core object shouldn't have a
            // path already created
            assert(m_dataBindPath == nullptr);
            m_dataBindPath = dataBindPath;
        }
    }
}

void DataBindPathReferencer::decodeDataBindPath(Span<const uint8_t>& value)
{
    m_dataBindPath = new DataBindPath();
    m_dataBindPath->decodePath(value);
    m_dataBindPath->resolved(true);
}