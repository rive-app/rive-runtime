#include "rive/animation/state_machine_event.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/generated/animation/state_machine_base.hpp"
#include "rive/artboard.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/event_input_change.hpp"

using namespace rive;

void StateMachineEvent::addInputChange(EventInputChange* inputChange) {
    m_InputChanges.push_back(inputChange);
}

StatusCode StateMachineEvent::import(ImportStack& importStack) {
    auto stateMachineImporter = importStack.latest<StateMachineImporter>(StateMachineBase::typeKey);
    if (stateMachineImporter == nullptr) {
        return StatusCode::MissingObject;
    }
    // Handing off ownership of this!
    stateMachineImporter->addEvent(std::unique_ptr<StateMachineEvent>(this));
    return Super::import(importStack);
}

const EventInputChange* StateMachineEvent::inputChange(size_t index) const {
    if (index < m_InputChanges.size()) {
        return m_InputChanges[index];
    }
    return nullptr;
}

StatusCode StateMachineEvent::onAddedClean(CoreContext* context) {
    auto artboard = static_cast<Artboard*>(context);
    auto target = artboard->resolve(targetId());

    for (auto core : artboard->objects()) {
        if (core == nullptr) {
            continue;
        }

        // Iterate artboard to find Shapes that are parented to the target
        if (core->is<Shape>()) {
            auto shape = core->as<Shape>();

            for (ContainerComponent* component = shape; component != nullptr;
                 component = component->parent()) {
                if (component == target) {
                    auto index = artboard->idOf(shape);
                    if (index != 0) {
                        m_HitShapesIds.push_back(artboard->idOf(shape));
                    }
                    break;
                }
            }
        }
    }

    return Super::onAddedClean(context);
}

void StateMachineEvent::performChanges(StateMachineInstance* stateMachineInstance) const {
    for (auto inputChange : m_InputChanges) {
        inputChange->perform(stateMachineInstance);
    }
}