#ifndef _RIVE_DATA_BIND_PATH_IMPORTER_HPP_
#define _RIVE_DATA_BIND_PATH_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class Core;
class DataBindPath;
class DataBindPathImporter : public ImportStackObject
{
private:
    DataBindPath* m_dataBindPath;

public:
    DataBindPathImporter(DataBindPath* object);
    DataBindPath* claim();
};
} // namespace rive
#endif
