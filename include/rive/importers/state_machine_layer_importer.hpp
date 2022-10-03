#ifndef _RIVE_STATE_MACHINE_LAYER_IMPORTER_HPP_
#define _RIVE_STATE_MACHINE_LAYER_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class StateMachineLayer;
class LayerState;
class Artboard;

class StateMachineLayerImporter : public ImportStackObject
{
private:
    StateMachineLayer* m_Layer;
    const Artboard* m_Artboard;

public:
    StateMachineLayerImporter(StateMachineLayer* layer, const Artboard* artboard);
    void addState(LayerState* state);
    StatusCode resolve() override;
    bool readNullObject() override;
};
} // namespace rive
#endif
