#ifndef _RIVE_SCRIPTED_OBJECT_IMPORTER_HPP_
#define _RIVE_SCRIPTED_OBJECT_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"
#include <cstdint>

namespace rive
{
class Core;
class CustomProperty;
class ScriptedObject;

class ScriptedObjectImporter : public ImportStackObject
{
private:
    ScriptedObject* m_scriptedObject;
    uint32_t m_inputParentId;

public:
    // inputParentId is the parent the exporter gives this object's inputs: its
    // artboard index when it is a Component, 0 otherwise.
    ScriptedObjectImporter(ScriptedObject* object, uint32_t inputParentId);
    ScriptedObject* scriptedObject() const { return m_scriptedObject; }
    uint32_t inputParentId() const { return m_inputParentId; }
    void addInput(CustomProperty* input);
    StatusCode resolve() override;
};
} // namespace rive
#endif