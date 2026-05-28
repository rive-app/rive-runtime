#ifndef _RIVE_GAMEPAD_BATCH_HPP_
#define _RIVE_GAMEPAD_BATCH_HPP_

#include <cstddef>
#include <cstdint>

namespace rive
{

/// Wire format version for `StateMachineInstance::submitGamepadsFromBuffer` /
/// JS `GAMEPAD_BATCH_VERSION` (version `uint32` then record stream, LE).
constexpr uint32_t kGamepadBatchWireVersion = 2;
constexpr uint8_t kGamepadBatchMaxButtons = 32;
constexpr uint8_t kGamepadBatchMaxAxes = 16;

/// Record `uint8` type after the version field.
enum class GamepadRecordType : uint8_t
{
    connected = 0,
    update = 1,
    disconnected = 2,
};

} // namespace rive

#endif
