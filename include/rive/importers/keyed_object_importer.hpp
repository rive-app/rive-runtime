#ifndef _RIVE_KEYED_OBJECT_IMPORTER_HPP_
#define _RIVE_KEYED_OBJECT_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class Core;
class KeyedObject;
class KeyedProperty;
class KeyedObjectImporter : public ImportStackObject
{
private:
    KeyedObject* m_KeyedObject;

public:
    KeyedObjectImporter(KeyedObject* keyedObject);
    void addKeyedProperty(std::unique_ptr<KeyedProperty>);
};
} // namespace rive
#endif
