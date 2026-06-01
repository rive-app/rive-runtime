#pragma once

#include <cstdint>

namespace rive
{

// Enum describing the format of a GPU-uploadable texture. Only formats which
// the GPU samples directly are listed. Lives at the `rive/` root so both the
// decoders library (which produces these tags) and the renderer (which
// consumes them) can include it without cross-layer dependencies.
enum class GPUTextureFormat : uint8_t
{
    rgba32,
    bc1,
    bc2,
    bc3,
    bc7,
    astc,
    etc2,
};

} // namespace rive
