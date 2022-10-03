#ifndef _RIVE_CUBIC_VERTEX_HPP_
#define _RIVE_CUBIC_VERTEX_HPP_
#include "rive/generated/shapes/cubic_vertex_base.hpp"
#include "rive/math/vec2d.hpp"

namespace rive
{
class Vec2D;
class CubicVertex : public CubicVertexBase
{
protected:
    bool m_InValid = false;
    bool m_OutValid = false;
    Vec2D m_InPoint;
    Vec2D m_OutPoint;

    virtual void computeIn() = 0;
    virtual void computeOut() = 0;

public:
    const Vec2D& outPoint();
    const Vec2D& inPoint();
    const Vec2D& renderOut();
    const Vec2D& renderIn();

    void outPoint(const Vec2D& value);
    void inPoint(const Vec2D& value);
    void xChanged() override;
    void yChanged() override;

    void deform(const Mat2D& worldTransform, const float* boneTransforms) override;
};
} // namespace rive

#endif