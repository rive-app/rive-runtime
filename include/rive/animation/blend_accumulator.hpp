#ifndef _RIVE_BLEND_ACCUMULATOR_HPP_
#define _RIVE_BLEND_ACCUMULATOR_HPP_

#include "rive/shapes/paint/color.hpp"
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace rive
{
class Artboard;
class Core;
class LinearAnimation;

// Mixes value into current the way a keyframe applied at mix does. The one
// copy of the formula: keyframes applied straight to their objects and the
// accumulator below both use it, so the two round the same.
inline float mixKeyedDouble(float current, float value, float mix)
{
  if (mix == 1.0f)
  {
    return value;
  }
  float mixi = 1.0f - mix;
  return current * mixi + value * mix;
}

// Collects what animations write, so each property is set once, to its final
// value. Applied straight to the objects, a property written more than once a
// frame went through intermediate values, each of which marked it changed and
// dirtied everything depending on it, even when the frame came out where it
// already was. Setting only the final value lets an unchanged property stay
// clean.
//
// Values mix exactly as they would on the objects: the first write since the
// last flush starts from the reset value when there is one, else from the
// object's current value.
//
// Two uses:
// - A blend state's (blend mode) collects every number and color its
//   animations write and flushes them when it has applied.
// - A state machine's (layer mode, see StateMachineInstance) handles only the
//   properties it tracks: those that states in two or more of its layers key.
//   Its layers write through it as they apply, one after the other, and it
//   flushes once they have all applied, or before anything else could read
//   or write those properties. Only what was written more than once before
//   the last flush is collected; a property written once, as most are, goes
//   straight to its object, as it would have without the accumulator. A
//   blend state in such a machine forwards the tracked properties to it (see
//   parent). Besides numbers and colors it handles discrete values (uints,
//   ids, bools, strings), which keyframes write at any mix: the last write
//   wins, as it would on the object.
class BlendAccumulator
{
public:
  // How a collected property's value is stored and set.
  enum class Kind : uint8_t
  {
    number,
    color,
    uint,
    id,
    boolean,
    string,
  };

  // The key a property is collected under. Keys that set the same value (a
  // node's x and xArtboard) share it, so their writes stay in order and the
  // set at the flush notifies the key the setter does.
  static int storageKey(int propertyKey);

  // Starts this frame's value for a property at its reset value.
  void seedDouble(Core* object, int propertyKey, float value);
  void seedColor(Core* object, int propertyKey, ColorInt value);

  // Mixes a keyframe value in. False when a layer mode accumulator doesn't
  // track the property, which the caller then applies to the object itself.
  bool applyDouble(Core* object, int propertyKey, float mix, float value);
  bool applyColor(Core* object, int propertyKey, float mix, ColorInt value);
  // Writes a discrete value (a uint, id or bool, as bits), replacing what
  // was written before. False when the machine's accumulator doesn't track
  // the property; a blend state's forwards only the ones it tracks.
  bool applyDiscrete(Core* object, int propertyKey, uint32_t value);
  bool applyString(Core* object, int propertyKey, const std::string& value);

  // Sets every property written since the last flush, in the order they
  // were first written.
  void flush();

  // Layer mode: from here on only these properties are collected. Each
  // write that changes a collected value moves [artboard]'s change serial
  // and tells the object's binds, as setting it would have: a pose resting
  // on the property must re-apply, and a bind reading it back must run, as
  // they would have with the intermediate values set.
  // Properties are tracked object by object: all of one object's before the
  // next one's.
  void notifyChangesTo(Artboard* artboard);
  void track(Core* object, int propertyKey, Kind kind);
  // Layer mode: which of [animation]'s keyed objects have a tracked property,
  // by index, worked out on first use. Null in blend mode, which handles
  // everything.
  const std::vector<bool>* trackedObjects(const LinearAnimation* animation);
  // Whether any of [animation]'s keyed objects has a tracked property.
  bool tracksAny(const LinearAnimation* animation);

  // Blend mode: properties [parent] tracks are written to it, at full mix,
  // instead of to their objects, and start from its value when it has one.
  void parent(BlendAccumulator* parent)
  {
    assert(m_layer == nullptr);
    m_parent = parent;
  }

  // Whether anything was written since the last flush.
  bool hasPending() const { return !m_written.empty(); }

private:
  struct Value
  {
    Core* object;
    int propertyKey;
    Kind kind;
    // Layer mode: writes since the flush this value was last written before
    // (at most 2), and whether those writes are collected.
    uint8_t writes;
    bool collecting;
    uint32_t frame;
    float doubleValue;
    // A color, uint, id or bool, or a string's index in LayerMode::strings;
    // in an object's marker (see track), how many properties it has.
    uint32_t bits;
  };

  // What only a layer mode accumulator needs.
  struct LayerMode
  {
    Artboard* artboard = nullptr;
    // The tracked strings' values.
    std::vector<std::string> strings;
    // The object the last write was to, and where its tracked properties
    // sit in m_values (none when count is 0): keyframes write an object's
    // properties one after the other, and most objects have none.
    const Core* lastObject = nullptr;
    uint32_t lastFirst = 0;
    uint32_t lastCount = 0;
    // See trackedObjects. A machine plays few animations.
    std::vector<std::pair<const LinearAnimation*, std::vector<bool>>> masks;
  };

  // Blend mode: this frame's value for the property, added (and loaded) on
  // the first write of the frame.
  Value* blendValue(Core* object, int propertyKey, Kind kind, bool seeding);
  // Layer mode: the tracked value this write is collected into, or null
  // when it isn't tracked or goes straight to the object.
  Value* layerValue(Core* object, int propertyKey);
  Value* trackedValue(const Core* object, int propertyKey);
  void load(Value& value, const Value* from);
  // The value collected since the last flush, or null.
  const Value* pending(const Core* object, int propertyKey);
  uint32_t find(const Core* object, int propertyKey) const;
  uint32_t add(Core* object, int propertyKey, Kind kind);
  void index(uint32_t valueIndex);
  void changed(const Value& value);
  void set(const Value& value);

  // Kept across frames: a blend writes the same properties every frame, so
  // after the first one nothing is allocated.
  std::vector<Value> m_values;
  // Open addressing over m_values: holds index + 1, 0 when empty. Its size
  // is a power of two, at least twice the number of values.
  std::vector<uint32_t> m_index;
  std::vector<uint32_t> m_written;
  // A blend mode accumulator's parent.
  BlendAccumulator* m_parent = nullptr;
  // Set in layer mode, which this marks.
  std::unique_ptr<LayerMode> m_layer;
  uint32_t m_frame = 1;
};
} // namespace rive
#endif
