#ifndef _RIVE_WASM_ARTBOARD_WIRE_HPP_
#define _RIVE_WASM_ARTBOARD_WIRE_HPP_

#include <cstdint>

namespace rive
{

/// Enum codes for the rive_artboard_v1 and dynamic rive_data_v1 ops; both
/// sides of the seam compile against these values.
struct ArtboardWire
{
    static constexpr uint32_t pointerDown = 0;
    static constexpr uint32_t pointerMove = 1;
    static constexpr uint32_t pointerUp = 2;
    static constexpr uint32_t pointerExit = 3;

    static constexpr uint32_t timeSeconds = 0;
    static constexpr uint32_t timeFrames = 1;
    static constexpr uint32_t timePercentage = 2;

    static constexpr uint32_t nodeX = 0;
    static constexpr uint32_t nodeY = 1;
    static constexpr uint32_t nodeRotation = 2;
    static constexpr uint32_t nodeScaleX = 3;
    static constexpr uint32_t nodeScaleY = 4;
    static constexpr uint32_t nodePosition = 5;
    static constexpr uint32_t nodeScale = 6;
};

/// Property kinds rive_data_v1.vmi_property reports for vm.name reads.
struct DataPropertyWire
{
    static constexpr uint32_t kindNone = 0;
    static constexpr uint32_t kindNumber = 1;
    static constexpr uint32_t kindBoolean = 2;
    static constexpr uint32_t kindString = 3;
    static constexpr uint32_t kindTrigger = 4;
    static constexpr uint32_t kindColor = 5;
    static constexpr uint32_t kindViewModel = 6;
    static constexpr uint32_t kindList = 7;
    static constexpr uint32_t kindEnum = 8;
    static constexpr uint32_t kindImage = 9;
    static constexpr uint32_t kindFont = 10;
    static constexpr uint32_t kindBlob = 11;
    static constexpr uint32_t kindSymbolListIndex = 12;
};

} // namespace rive

#endif
