#ifndef _RIVE_LAYER_MASK_MODE_HPP_
#define _RIVE_LAYER_MASK_MODE_HPP_

#include <cstdint>

namespace rive
{
// How a layer mask's rasterized coverage becomes a multiplier on the content it
// masks. Mirrors the enumValues order in dev/defs/layer_mask.json and
// rive::MaskMode, which is what the serialized maskModeValue indexes into, and
// the LAYER_MASK_MODE_* constants the shaders use.
//
// Its own header (rather than renderer.hpp) so the GPU layer can name it
// without pulling in the whole Renderer interface, the same way blend_mode.hpp
// works.
enum class LayerMaskMode : uint8_t
{
    alpha = 0,             // f = mask.a
    invertedAlpha = 1,     // f = 1 - mask.a
    luminance = 2,         // f = luma(mask.rgb)
    invertedLuminance = 3, // f = 1 - luma(mask.rgb)
};
} // namespace rive
#endif
