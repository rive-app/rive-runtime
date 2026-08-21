#ifdef WITH_RIVE_SCRIPTING
#include "rive/scripted/script_backend.hpp"
#include "rive/scripted/scripted_object.hpp"

using namespace rive;

ScriptBackend::~ScriptBackend() { detachScriptedObjects(); }

void ScriptBackend::registerScriptedObject(ScriptedObject* object)
{
    if (object != nullptr)
    {
        m_scriptedObjects.insert(object);
    }
}

void ScriptBackend::unregisterScriptedObject(ScriptedObject* object)
{
    if (object != nullptr)
    {
        m_scriptedObjects.erase(object);
    }
}

void ScriptBackend::detachScriptedObjects()
{
    for (ScriptedObject* object : m_scriptedObjects)
    {
        object->m_vm = nullptr;
    }
    m_scriptedObjects.clear();
}
#endif
