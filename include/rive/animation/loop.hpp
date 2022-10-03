#ifndef _RIVE_LOOP_HPP_
#define _RIVE_LOOP_HPP_
namespace rive
{
/// Loop options for linear animations.
enum class Loop : unsigned int
{
    /// Play until the duration or end of work area of the animation.
    oneShot = 0,

    /// Play until the duration or end of work area of the animation and
    /// then go back to the start (0 seconds).
    loop = 1,

    /// Play to the end of the duration/work area and then play back.
    pingPong = 2
};
} // namespace rive
#endif
