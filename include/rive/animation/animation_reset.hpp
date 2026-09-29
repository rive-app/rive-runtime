#ifndef _RIVE_ANIMATION_RESET_HPP_
#define _RIVE_ANIMATION_RESET_HPP_

#include <string>
#include "rive/artboard.hpp"
#include "rive/animation/animation_reset.hpp"
#include "rive/core/binary_writer.hpp"
#include "rive/core/vector_binary_writer.hpp"
#include "rive/core/binary_data_reader.hpp"

namespace rive
{
class BlendAccumulator;

class AnimationReset
{
private:
    VectorBinaryWriter m_binaryWriter;
    BinaryDataReader m_binaryReader;
    std::vector<uint8_t> m_WriteBuffer;
    // Calls visit(object, propertyKey, value) for every stored value.
    template <typename Visit>
    void forEachValue(Artboard* artboard, Visit&& visit);

public:
    AnimationReset();
    void writeObjectId(uint32_t objectId);
    void writeTotalProperties(uint32_t value);
    void writePropertyKey(uint32_t value);
    void writePropertyValue(float value);
    void apply(Artboard* artboard);
    // Feeds the reset values to [accumulator] instead of the objects.
    void seed(Artboard* artboard, BlendAccumulator& accumulator);
    void complete();
    void clear();
};
} // namespace rive
#endif
