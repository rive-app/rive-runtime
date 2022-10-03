#ifndef _RIVE_STROKE_JOIN_HPP_
#define _RIVE_STROKE_JOIN_HPP_
namespace rive
{
/// Style used for stroke segment joins when there is a sharp change.
enum class StrokeJoin : unsigned int
{
    /// Makes a sharp corner at the joint.
    miter = 0,

    /// Smoothens the joint with a circular/semi-circular shape at the
    /// joint.
    round = 1,

    /// Creates a beveled edge at the joint.
    bevel = 2
};
} // namespace rive
#endif
