#ifndef _RIVE_LAYOUT_HPP_
#define _RIVE_LAYOUT_HPP_
namespace rive
{
enum class Fit : unsigned char
{
    fill,
    contain,
    cover,
    fitWidth,
    fitHeight,
    none,
    scaleDown
};

class Alignment
{
public:
    Alignment(float x, float y) : m_x(x), m_y(y) {}
    Alignment() : m_x(0.0f), m_y(0.0f) {}

    float x() const { return m_x; }
    float y() const { return m_y; }

    static const Alignment topLeft;
    static const Alignment topCenter;
    static const Alignment topRight;
    static const Alignment centerLeft;
    static const Alignment center;
    static const Alignment centerRight;
    static const Alignment bottomLeft;
    static const Alignment bottomCenter;
    static const Alignment bottomRight;

private:
    float m_x, m_y;
};

} // namespace rive
#endif