#ifndef _RIVE_SCRIPTED_OBJECT_IMPORTER_HPP_
#define _RIVE_SCRIPTED_OBJECT_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class Core;
class CustomProperty;
class ScriptedObject;

class ScriptedObjectImporter : public ImportStackObject
{
private:
    ScriptedObject* m_scriptedObject;

public:
    ScriptedObjectImporter(ScriptedObject* object);
    void addInput(CustomProperty* input);
    StatusCode resolve() override;
};
} // namespace rive
#endif