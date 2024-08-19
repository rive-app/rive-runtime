#ifndef _RIVE_DATA_CONVERTER_GROUP_IMPORTER_HPP_
#define _RIVE_DATA_CONVERTER_GROUP_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class Core;
class DataConverterGroup;
class DataConverterGroupImporter : public ImportStackObject
{
private:
    DataConverterGroup* m_dataConverterGroup;

public:
    DataConverterGroupImporter(DataConverterGroup* group);
    DataConverterGroup* group() { return m_dataConverterGroup; }
};
} // namespace rive
#endif