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
private:
    float m_X, m_Y;

public:
    Alignment(float x, float y) : m_X(x), m_Y(y) {}

    float x() const { return m_X; }
    float y() const { return m_Y; }

    static const Alignment topLeft;
    static const Alignment topCenter;
    static const Alignment topRight;
    static const Alignment centerLeft;
    static const Alignment center;
    static const Alignment centerRight;
    static const Alignment bottomLeft;
    static const Alignment bottomCenter;
    static const Alignment bottomRight;
};

} // namespace rive
#endif