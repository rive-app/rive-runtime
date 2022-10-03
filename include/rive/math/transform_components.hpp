#ifndef _RIVE_TRANSFORMCOMPONENTS_HPP_
#define _RIVE_TRANSFORMCOMPONENTS_HPP_

#include "rive/math/vec2d.hpp"

namespace rive
{
class TransformComponents
{
private:
    float m_X;
    float m_Y;
    float m_ScaleX;
    float m_ScaleY;
    float m_Rotation;
    float m_Skew;

public:
    TransformComponents() :
        m_X(0.0f), m_Y(0.0f), m_ScaleX(1.0f), m_ScaleY(1.0f), m_Rotation(0.0f), m_Skew(0.0f)
    {}
    TransformComponents(const TransformComponents& copy) :
        m_X(copy.m_X),
        m_Y(copy.m_Y),
        m_ScaleX(copy.m_ScaleX),
        m_ScaleY(copy.m_ScaleY),
        m_Rotation(copy.m_Rotation),
        m_Skew(copy.m_Skew)
    {}

    float x() const { return m_X; }
    void x(float value) { m_X = value; }
    float y() const { return m_Y; }
    void y(float value) { m_Y = value; }
    float scaleX() const { return m_ScaleX; }
    void scaleX(float value) { m_ScaleX = value; }
    float scaleY() const { return m_ScaleY; }
    void scaleY(float value) { m_ScaleY = value; }
    float rotation() const { return m_Rotation; }
    void rotation(float value) { m_Rotation = value; }
    float skew() const { return m_Skew; }
    void skew(float value) { m_Skew = value; }

    Vec2D translation() const { return {m_X, m_Y}; }
    Vec2D scale() const { return {m_ScaleX, m_ScaleY}; }

    TransformComponents& operator=(const TransformComponents& a)
    {
        m_X = a.m_X;
        m_Y = a.m_Y;
        m_ScaleX = a.m_ScaleX;
        m_ScaleY = a.m_ScaleY;
        m_Rotation = a.m_Rotation;
        m_Skew = a.m_Skew;
        return *this;
    }
};
} // namespace rive
#endif
