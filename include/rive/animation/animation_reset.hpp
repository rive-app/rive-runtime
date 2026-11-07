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
class LinearAnimation;

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
  // Writes, for every object the animations key, the current value of each
  // double and color property they key, once per property: the first
  // animation's properties first, each object where it was first keyed.
  // With useFirstAsBaseline, the first animation's properties take their
  // first keyframe's value instead. Allocates nothing once this thread has
  // built a reset as big and this reset has held one as big (see
  // AnimationResetFactory's pool).
  void build(
      const LinearAnimation* const* animations,
      size_t count,
      Artboard* artboard,
      bool useFirstAsBaseline);
  void writeObjectId(uint32_t objectId);
  void writeTotalProperties(uint32_t value);
  void writePropertyKey(uint32_t value);
  void writePropertyValue(float value);
  // Sets the reset values, through [accumulator] for the properties it
  // tracks (see BlendAccumulator::track).
  void apply(Artboard* artboard, BlendAccumulator* accumulator = nullptr);
  // Feeds the reset values to [accumulator] instead of the objects.
  void seed(Artboard* artboard, BlendAccumulator& accumulator);
  void complete();
  void clear();
#ifdef TESTING
  // What has been written so far.
  std::vector<uint8_t> bytes() const
  {
    return std::vector<uint8_t>(
        m_WriteBuffer.begin(),
        m_WriteBuffer.begin() + m_binaryWriter.size());
  }
#endif
};
} // namespace rive
#endif
