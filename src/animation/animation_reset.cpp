#include "rive/animation/animation_reset.hpp"
#include "rive/core/vector_binary_writer.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

AnimationReset::AnimationReset() : m_binaryWriter(&m_WriteBuffer), m_binaryReader(nullptr, 0) {}

void AnimationReset::writeObjectId(uint32_t objectId) { m_binaryWriter.writeVarUint(objectId); }

void AnimationReset::writeTotalProperties(uint32_t value) { m_binaryWriter.writeVarUint(value); }

void AnimationReset::writePropertyKey(uint32_t value) { m_binaryWriter.writeVarUint(value); }

void AnimationReset::writePropertyValue(float value) { m_binaryWriter.writeFloat(value); }

void AnimationReset::clear() { m_binaryWriter.clear(); }

void AnimationReset::complete()
{
    m_binaryReader.complete(&m_WriteBuffer.front(), m_binaryWriter.size());
}

void AnimationReset::apply(Artboard* artboard)
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
            auto propertyValue = m_binaryReader.readFloat32();
            switch (CoreRegistry::propertyFieldId(propertyKey))
            {
                case CoreDoubleType::id:
                    CoreRegistry::setDouble(object, propertyKey, propertyValue);
                    break;
                case CoreColorType::id:
                    CoreRegistry::setColor(object, propertyKey, propertyValue);
                    break;
            }
            currentPropertyIndex++;
        }
    }
}
