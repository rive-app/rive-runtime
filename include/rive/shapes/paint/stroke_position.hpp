#ifndef _RIVE_STROKE_POSITION_HPP_
#define _RIVE_STROKE_POSITION_HPP_
namespace rive
{
/// Style used for stroke positioning.
enum class StrokePosition : unsigned int
{
    /// Render the stroke inside the path's edge.
    inside = 0,

    /// Center the stroke on the path's edge.
    center = 1,

    /// Render the stroke completely outside of the path.
    outside = 2
};
} // namespace rive
#endif
