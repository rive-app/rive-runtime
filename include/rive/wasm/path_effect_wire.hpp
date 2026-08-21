#ifndef _RIVE_WASM_PATH_EFFECT_WIRE_HPP_
#define _RIVE_WASM_PATH_EFFECT_WIRE_HPP_

#include <cstdint>

namespace rive
{

/// Host to module layout for callPathEffectUpdate's paint argument: the
/// exact ShapePaint subset the Luau lane's PaintData snapshot captures
/// (style, stroke thickness/cap/join, first solid color, feather strength,
/// blend mode). Gradients and the node's live transform surface stay host
/// side. The host writes every field; enum fields carry the runtime enum
/// values both sides compile against.
struct PathEffectPaintWire
{
    uint32_t style = 0;
    uint32_t join = 0;
    uint32_t cap = 0;
    uint32_t blendMode = 0;
    uint32_t color = 0;
    float thickness = 0;
    float feather = 0;
};

} // namespace rive

#endif
