#ifndef _RIVE_ARTBOARD_IMPORTER_HPP_
#define _RIVE_ARTBOARD_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"
#include <cstddef>

namespace rive
{
class Core;
class Artboard;
class LinearAnimation;
class StateMachine;
class TextValueRun;
class Event;
class DataBind;
class ArtboardImporter : public ImportStackObject
{
private:
    Artboard* m_Artboard;

public:
    ArtboardImporter(Artboard* artboard);
    // Appends object (or an empty slot) and returns its index.
    size_t addComponent(Core* object);
    // Empties a slot whose object failed to import. The index stays taken so
    // every later id still lines up; the object itself is freed elsewhere.
    void releaseSlot(size_t index);
    void addAnimation(LinearAnimation* animation);
    void addStateMachine(StateMachine* stateMachine);
    void addDataBind(DataBind* dataBind);
    StatusCode resolve() override;
    const Artboard* artboard() const { return m_Artboard; }

    bool readNullObject() override;
};
} // namespace rive
#endif
