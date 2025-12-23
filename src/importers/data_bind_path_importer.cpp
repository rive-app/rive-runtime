#include "rive/data_bind/data_bind_path.hpp"
#include "rive/importers/data_bind_path_importer.hpp"

using namespace rive;

DataBindPathImporter::DataBindPathImporter(DataBindPath* obj) :
    m_dataBindPath(obj)
{}

DataBindPath* DataBindPathImporter::claim()
{
    auto path = m_dataBindPath;
    m_dataBindPath = nullptr;
    return path;
}