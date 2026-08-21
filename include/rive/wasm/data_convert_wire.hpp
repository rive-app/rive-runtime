#ifndef _RIVE_WASM_DATA_CONVERT_WIRE_HPP_
#define _RIVE_WASM_DATA_CONVERT_WIRE_HPP_

#include <cstdint>

namespace rive
{

/// Host to module layout for callDataConvert's input value; the same kind
/// codes ride the rive_data_convert_result import back out. Kind values
/// match ScriptBackend::ScriptDataResult::Kind. The string kind appends
/// stringLength bytes after the struct.
struct DataConvertWire
{
    static constexpr uint32_t kindNone = 0;
    static constexpr uint32_t kindNumber = 1;
    static constexpr uint32_t kindString = 2;
    static constexpr uint32_t kindBoolean = 3;
    static constexpr uint32_t kindColor = 4;

    uint32_t kind = kindNone;
    float number = 0;
    uint32_t boolean = 0;
    int32_t color = 0;
    uint32_t stringLength = 0;
};

} // namespace rive

#endif
