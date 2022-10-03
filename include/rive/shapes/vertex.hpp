#ifndef _RIVE_VERTEX_HPP_
#define _RIVE_VERTEX_HPP_
#include "rive/bones/weight.hpp"
#include "rive/generated/shapes/vertex_base.hpp"
#include "rive/math/mat2d.hpp"
namespace rive
{
class Vertex : public VertexBase
{
    friend class Weight;

private:
    Weight* m_Weight = nullptr;
    void weight(Weight* value) { m_Weight = value; }

public:
    template <typename T> T* weight() { return m_Weight->as<T>(); }
    virtual void deform(const Mat2D& worldTransform, const float* boneTransforms);
    bool hasWeight() { return m_Weight != nullptr; }
    Vec2D renderTranslation();

protected:
    virtual void markGeometryDirty() = 0;
    void xChanged() override;
    void yChanged() override;

#ifdef TESTING
public:
    Weight* weight() { return m_Weight; }
#endif
};
} // namespace rive

#endif