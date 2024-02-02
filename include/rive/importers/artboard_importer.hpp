#ifndef _RIVE_ARTBOARD_IMPORTER_HPP_
#define _RIVE_ARTBOARD_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class Core;
class Artboard;
class LinearAnimation;
class StateMachine;
class TextValueRun;
class Event;
class ArtboardImporter : public ImportStackObject
{
private:
    Artboard* m_Artboard;

public:
    ArtboardImporter(Artboard* artboard);
    void addComponent(Core* object);
    void addAnimation(LinearAnimation* animation);
    void addStateMachine(StateMachine* stateMachine);
    StatusCode resolve() override;
    const Artboard* artboard() const { return m_Artboard; }

    bool readNullObject() override;
};
} // namespace rive
#endif
