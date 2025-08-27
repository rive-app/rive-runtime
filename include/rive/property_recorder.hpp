#ifndef _RIVE_PROPERTY_RECORDER_HPP_
#define _RIVE_PROPERTY_RECORDER_HPP_

#include "rive/core/binary_writer.hpp"
#include "rive/core/vector_binary_writer.hpp"
#include "rive/core/binary_data_reader.hpp"
#include "rive/core/binary_data_reader.hpp"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace rive
{
class Artboard;
class StateMachine;
class StateMachineLayer;
class StateMachineInput;
class LayerState;
class LinearAnimation;
class KeyedObject;
class Core;
class StateMachineInstance;
class CoreObjectData
{
private:
    std::vector<uint16_t> m_propertyKeys;

public:
    CoreObjectData(uint32_t id) { objectId = id; }
    uint32_t objectId;
    void addPropertyKey(uint16_t key)
    {
        auto it = std::find(m_propertyKeys.begin(), m_propertyKeys.end(), key);
        if (it == m_propertyKeys.end())
        {
            m_propertyKeys.push_back(key);
        }
    }
    std::vector<uint16_t>* propertyKeys() { return &m_propertyKeys; }
};

class PropertyRecorder
{
private:
    VectorBinaryWriter m_binaryWriter;
    BinaryDataReader m_binaryReader;
    std::vector<uint8_t> m_WriteBuffer;
    VectorBinaryWriter m_binaryWriterSM;
    BinaryDataReader m_binaryReaderSM;
    std::vector<uint8_t> m_WriteBufferSM;
    std::vector<std::unique_ptr<CoreObjectData>> m_coreObjectsData;
    void recordDataBinds(const Artboard* artboard);
    void recordStateMachine(const StateMachine* stateMachine);
    void recordStateMachineInput(const StateMachineInput* stateMachineInput);
    void recordStateMachineLayer(const StateMachineLayer* stateMachineLayer);
    void recordStateMachineLayerState(const LayerState* layerState);
    void recordLinearAnimation(const LinearAnimation* linearAnimation);
    void recordKeyedObject(const KeyedObject* keyedObject);
    CoreObjectData* getCoreObjectData(uint32_t id);
    void writeProperties(const Artboard* artboard);
    void addPropertyKey(CoreObjectData* coreObjectData, int propertyKey);
    int getObjectId(const Artboard* artboard, Core* object);
    void writeObjectId(uint32_t objectId, VectorBinaryWriter& writer);
    void writeTotalProperties(uint32_t value, VectorBinaryWriter& writer);
    void writePropertyKey(uint32_t value, VectorBinaryWriter& writer);
    void writePropertyValue(float value, VectorBinaryWriter& writer);
    void writePropertyValue(int value, VectorBinaryWriter& writer);
    void writePropertyValue(uint32_t value, VectorBinaryWriter& writer);
    void writePropertyValue(std::string value, VectorBinaryWriter& writer);
    void writePropertyValue(bool value, VectorBinaryWriter& writer);
    void writeSeparator();
    void complete(VectorBinaryWriter& writer,
                  BinaryDataReader& reader,
                  std::vector<uint8_t>& buffer);

public:
    PropertyRecorder();
    void recordArtboard(const Artboard* artboard);
    void recordStateMachineInputs(const StateMachine* stateMachine);
    void apply(Artboard* artboard);
    void apply(StateMachineInstance* stateMachineInstace);
    void clear();
};
} // namespace rive
#endif
