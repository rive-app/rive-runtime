#ifndef _RIVE_STROKE_CAP_HPP_
#define _RIVE_STROKE_CAP_HPP_
namespace rive
{
/// Style used for stroke line endings.
enum class StrokeCap : unsigned int
{
    /// Flat edge at the start/end of the stroke.
    butt = 0,

    /// Circular edge at the start/end of the stroke.
    round = 1,

    /// Flat protruding edge at the start/end of the stroke.
    square = 2
};
} // namespace rive
#endif
