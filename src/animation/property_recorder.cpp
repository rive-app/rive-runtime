#include "rive/property_recorder.hpp"
#include "rive/core/vector_binary_writer.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/artboard.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_input.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/animation/state_machine_bool.hpp"
#include "rive/animation/state_machine_trigger.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/layer_state.hpp"
#include "rive/animation/blend_state.hpp"
#include "rive/animation/keyed_object.hpp"

using namespace rive;

PropertyRecorder::PropertyRecorder() :
    m_binaryWriter(&m_WriteBuffer),
    m_binaryReader(nullptr, 0),
    m_binaryWriterSM(&m_WriteBufferSM),
    m_binaryReaderSM(nullptr, 0)
{}

void PropertyRecorder::writeObjectId(uint32_t objectId,
                                     VectorBinaryWriter& writer)
{
    writer.writeVarUint(objectId);
}

void PropertyRecorder::writeTotalProperties(uint32_t value,
                                            VectorBinaryWriter& writer)
{
    writer.writeVarUint(value);
}

void PropertyRecorder::writePropertyKey(uint32_t value,
                                        VectorBinaryWriter& writer)
{
    writer.writeVarUint(value);
}

void PropertyRecorder::writePropertyValue(float value,
                                          VectorBinaryWriter& writer)
{
    writer.writeFloat(value);
}

void PropertyRecorder::writePropertyValue(int value, VectorBinaryWriter& writer)
{
    writer.writeVarUint((uint32_t)value);
}

void PropertyRecorder::writePropertyValue(uint32_t value,
                                          VectorBinaryWriter& writer)
{
    writer.writeVarUint(value);
}

void PropertyRecorder::writePropertyValue(std::string value,
                                          VectorBinaryWriter& writer)
{
    writer.write(value);
}

void PropertyRecorder::writePropertyValue(bool value,
                                          VectorBinaryWriter& writer)
{
    writer.write((uint8_t)value);
}

void PropertyRecorder::clear() { m_binaryWriter.clear(); }

void PropertyRecorder::complete(VectorBinaryWriter& writer,
                                BinaryDataReader& reader,
                                std::vector<uint8_t>& buffer)
{
    reader.complete(&buffer.front(), writer.size());
}

void PropertyRecorder::recordArtboard(const Artboard* artboard)
{
    auto sm = artboard->stateMachine(0);
    recordStateMachineInputs(sm);
    recordStateMachine(sm);
    recordDataBinds(artboard);
    writeProperties(artboard);
    complete(m_binaryWriter, m_binaryReader, m_WriteBuffer);
}

void PropertyRecorder::recordDataBinds(const Artboard* artboard)
{
    auto dataBinds = artboard->dataBinds();
    for (auto& dataBind : dataBinds)
    {
        auto target = dataBind->target();
        auto index = getObjectId(artboard, target);
        auto propertyKey = dataBind->propertyKey();
        if (index >= 0)
        {
            auto coreObjectData = getCoreObjectData((uint32_t)index);
            addPropertyKey(coreObjectData, propertyKey);
        }
    }
}

void PropertyRecorder::addPropertyKey(CoreObjectData* coreObjectData,
                                      int propertyKey)
{
    switch (CoreRegistry::propertyFieldId(propertyKey))
    {
        case CoreDoubleType::id:
        case CoreColorType::id:
        case CoreUintType::id:
        case CoreStringType::id:
        case CoreBoolType::id:
            coreObjectData->addPropertyKey(propertyKey);
            break;
        default:
            break;
    }
}
void PropertyRecorder::recordStateMachine(const StateMachine* stateMachine)
{
    if (stateMachine != nullptr)
    {
        auto totalLayers = stateMachine->layerCount();
        for (size_t i = 0; i < totalLayers; i++)
        {
            recordStateMachineLayer(stateMachine->layer(i));
        }
    }
}
void PropertyRecorder::recordStateMachineInputs(
    const StateMachine* stateMachine)
{
    if (stateMachine != nullptr)
    {
        auto totalInputs = stateMachine->inputCount();
        for (size_t i = 0; i < totalInputs; i++)
        {
            recordStateMachineInput(stateMachine->input(i));
        }
    }
    complete(m_binaryWriterSM, m_binaryReaderSM, m_WriteBufferSM);
}
void PropertyRecorder::recordStateMachineInput(
    const StateMachineInput* stateMachineInput)
{
    if (stateMachineInput != nullptr)
    {
        if (stateMachineInput->is<StateMachineNumber>())
        {
            writePropertyValue(0, m_binaryWriterSM);
            writePropertyValue(
                stateMachineInput->as<StateMachineNumber>()->value(),
                m_binaryWriterSM);
        }
        else if (stateMachineInput->is<StateMachineBool>())
        {
            writePropertyValue(1, m_binaryWriterSM);
            writePropertyValue(
                stateMachineInput->as<StateMachineBool>()->value(),
                m_binaryWriterSM);
        }
        else if (stateMachineInput->is<StateMachineTrigger>())
        {
            // No need to write a value for triggers, but writing the type to
            // keep the indexes coherent
            writePropertyValue(2, m_binaryWriterSM);
        }
    }
}

void PropertyRecorder::recordStateMachineLayer(
    const StateMachineLayer* stateMachineLayer)
{

    auto totalStates = stateMachineLayer->stateCount();
    for (size_t j = 0; j < totalStates; j++)
    {
        auto state = stateMachineLayer->state(j);
        recordStateMachineLayerState(state);
    }
}
void PropertyRecorder::recordStateMachineLayerState(
    const LayerState* layerState)
{
    if (layerState->is<AnimationState>())
    {
        recordLinearAnimation(layerState->as<AnimationState>()->animation());
    }
    else if (layerState->is<BlendState>())
    {
        auto blendState = layerState->as<BlendState>();
        for (auto& blendAnimation : blendState->animations())
        {
            recordLinearAnimation(blendAnimation->animation());
        }
    }
}
void PropertyRecorder::recordLinearAnimation(
    const LinearAnimation* linearAnimation)
{
    if (linearAnimation != nullptr)
    {
        auto totalObjects = linearAnimation->numKeyedObjects();
        for (size_t i = 0; i < totalObjects; i += 1)
        {
            auto keyedObject = linearAnimation->getObject(i);
            recordKeyedObject(keyedObject);
        }
    }
}
void PropertyRecorder::recordKeyedObject(const KeyedObject* keyedObject)
{
    if (keyedObject != nullptr)
    {
        auto coreObjectData = getCoreObjectData(keyedObject->objectId());

        auto totalProperties = keyedObject->numKeyedProperties();
        for (size_t i = 0; i < totalProperties; i += 1)
        {
            auto property = keyedObject->getProperty(i);
            addPropertyKey(coreObjectData, property->propertyKey());
        }
    }
}

CoreObjectData* PropertyRecorder::getCoreObjectData(uint32_t id)
{
    for (auto& coreObjectData : m_coreObjectsData)
    {
        if (coreObjectData->objectId == id)
        {
            return coreObjectData.get();
        }
    }

    auto newCoreObjectData = rivestd::make_unique<CoreObjectData>(id);
    auto ref = newCoreObjectData.get();
    m_coreObjectsData.push_back(std::move(newCoreObjectData));
    return ref;
}

void PropertyRecorder::writeProperties(const Artboard* artboard)
{
    for (auto& coreObjectData : m_coreObjectsData)
    {
        auto propertyKeys = *coreObjectData->propertyKeys();
        if (propertyKeys.size() > 0)
        {
            auto object = artboard->resolve(coreObjectData->objectId);
            writeObjectId(coreObjectData->objectId, m_binaryWriter);
            writeTotalProperties((uint32_t)propertyKeys.size(), m_binaryWriter);
            for (auto& propertyKey : propertyKeys)
            {
                switch (CoreRegistry::propertyFieldId(propertyKey))
                {
                    case CoreDoubleType::id:
                    {
                        writePropertyKey(propertyKey, m_binaryWriter);
                        auto value =
                            CoreRegistry::getDouble(object, propertyKey);
                        writePropertyValue(value, m_binaryWriter);
                    }
                    break;
                    case CoreColorType::id:
                    {
                        writePropertyKey(propertyKey, m_binaryWriter);
                        auto value =
                            CoreRegistry::getColor(object, propertyKey);
                        writePropertyValue(value, m_binaryWriter);
                    }
                    case CoreUintType::id:
                    {
                        writePropertyKey(propertyKey, m_binaryWriter);
                        auto value = CoreRegistry::getUint(object, propertyKey);
                        writePropertyValue(value, m_binaryWriter);
                    }
                    break;
                    case CoreStringType::id:
                    {
                        writePropertyKey(propertyKey, m_binaryWriter);
                        auto value =
                            CoreRegistry::getString(object, propertyKey);
                        writePropertyValue(value, m_binaryWriter);
                    }
                    break;
                    case CoreBoolType::id:
                    {
                        writePropertyKey(propertyKey, m_binaryWriter);
                        auto value = CoreRegistry::getBool(object, propertyKey);
                        writePropertyValue(value, m_binaryWriter);
                    }
                    break;
                    default:
                        break;
                }
            }
        }
    }
}

int PropertyRecorder::getObjectId(const Artboard* artboard, Core* object)
{
    return artboard->objectIndex(object);
}

// void PropertyRecorder::applyInputs() {}

void PropertyRecorder::apply(StateMachineInstance* stateMachineInstace)
{
    int index = 0;
    m_binaryReaderSM.reset(&m_WriteBufferSM.front());
    while (!m_binaryReaderSM.isEOF() && index < 20)
    {
        auto inputType = m_binaryReaderSM.readVarUint32();
        auto smInput = stateMachineInstace->input(index);
        if (inputType == 0)
        {
            auto value = m_binaryReaderSM.readFloat32();
            auto smInputNumber =
                stateMachineInstace->getNumber(smInput->name());
            if (smInputNumber != nullptr)
            {
                smInputNumber->value(value);
            }
        }
        else if (inputType == 1)
        {
            auto value = m_binaryReaderSM.readByte();
            auto smInputBool = stateMachineInstace->getBool(smInput->name());
            if (smInputBool != nullptr)
            {
                smInputBool->value(value);
            }
        }
        index += 1;
    }
}

void PropertyRecorder::apply(Artboard* artboard)
{
    m_binaryReader.reset(&m_WriteBuffer.front());
    while (!m_binaryReader.isEOF())
    {
        auto objectId = m_binaryReader.readVarUint32();
        auto object = artboard->resolve(objectId);
        auto totalProperties = m_binaryReader.readVarUint32();
        uint32_t currentPropertyIndex = 0;
        while (currentPropertyIndex < totalProperties)
        {
            auto propertyKey = m_binaryReader.readVarUint32();
            switch (CoreRegistry::propertyFieldId(propertyKey))
            {
                case CoreDoubleType::id:
                {
                    auto propertyValue = m_binaryReader.readFloat32();
                    CoreRegistry::setDouble(object, propertyKey, propertyValue);
                    break;
                }
                case CoreColorType::id:
                {
                    auto propertyValue = m_binaryReader.readVarUint32();
                    CoreRegistry::setColor(object, propertyKey, propertyValue);
                    break;
                }
                case CoreUintType::id:
                {
                    auto propertyValue = m_binaryReader.readVarUint32();
                    CoreRegistry::setUint(object, propertyKey, propertyValue);
                    break;
                }
                case CoreStringType::id:
                {
                    auto propertyValue = m_binaryReader.readString();
                    CoreRegistry::setString(object, propertyKey, propertyValue);
                    break;
                }
                case CoreBoolType::id:
                {
                    auto propertyValue = m_binaryReader.readByte();
                    CoreRegistry::setBool(object,
                                          propertyKey,
                                          (bool)propertyValue);
                    break;
                }
            }
            currentPropertyIndex++;
        }
    }
}