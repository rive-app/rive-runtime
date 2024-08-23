#include "rive/artboard.hpp"
#include "rive/importers/data_converter_group_importer.hpp"
#include "rive/data_bind/converters/data_converter.hpp"

using namespace rive;

DataConverterGroupImporter::DataConverterGroupImporter(DataConverterGroup* group) :
    m_dataConverterGroup(group)
{}