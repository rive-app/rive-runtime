#ifndef _RIVE_BLEND_ACCUMULATOR_HPP_
#define _RIVE_BLEND_ACCUMULATOR_HPP_

#include "rive/shapes/paint/color.hpp"
#include <cstdint>
#include <vector>

namespace rive
{
class Core;

// Collects what a blend state's animations write, so each property is set
// once, to its final value. Applied straight to the objects, a blended
// property went through intermediate values every frame (the reset value, then
// one lerp per animation), each of which marked it changed and dirtied
// everything depending on it, even when the blend came out where it already
// was. Setting only the final value lets an unchanged property stay clean.
//
// Values mix exactly as they would on the objects: the first write of a frame
// starts from the reset value when there is one, else from the object's
// current value.
class BlendAccumulator
{
public:
    // Starts this frame's value for a property at its reset value.
    void seedDouble(Core* object, int propertyKey, float value);
    void seedColor(Core* object, int propertyKey, ColorInt value);

    void applyDouble(Core* object, int propertyKey, float mix, float value);
    void applyColor(Core* object, int propertyKey, float mix, ColorInt value);

    // Sets every property written since the last flush, in the order they
    // were first written.
    void flush();

private:
    struct Value
    {
        Core* object;
        int propertyKey;
        bool isColor;
        uint32_t frame;
        float doubleValue;
        ColorInt colorValue;
    };

    // This frame's value for the property, added (and loaded from the object)
    // on the first write of the frame.
    Value& value(Core* object, int propertyKey, bool isColor, bool seeding);
    uint32_t find(Core* object, int propertyKey) const;
    void index(uint32_t valueIndex);

    // Kept across frames: a blend writes the same properties every frame, so
    // after the first one nothing is allocated.
    std::vector<Value> m_values;
    // Open addressing over m_values: holds index + 1, 0 when empty. Its size
    // is a power of two, at least twice the number of values.
    std::vector<uint32_t> m_index;
    std::vector<uint32_t> m_written;
    uint32_t m_frame = 1;
};
} // namespace rive
#endif
