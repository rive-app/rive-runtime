#ifndef _RIVE_DEFORMER_HPP_
#define _RIVE_DEFORMER_HPP_

#include <vector>

namespace rive
{
class Component;
class RawPath;
class Mat2D;

class Deformer
{
public:
    virtual Component* asComponent() = 0;
    virtual ~Deformer() {}
};

class RenderPathDeformer : public Deformer
{
public:
    static RenderPathDeformer* from(Component* component);
    virtual void deformLocalRenderPath(RawPath& path,
                                       const Mat2D& worldTransform,
                                       const Mat2D& inverseWorld) const = 0;
    virtual void deformWorldRenderPath(RawPath& path) const = 0;

    virtual ~RenderPathDeformer() {}
};

class PointDeformer : public Deformer
{
public:
    static PointDeformer* from(Component* component);
    virtual Vec2D deformLocalPoint(Vec2D point,
                                   const Mat2D& worldTransform,
                                   const Mat2D& inverseWorld) const = 0;
    virtual Vec2D deformWorldPoint(Vec2D point) const = 0;

    virtual ~PointDeformer() {}
};

} // namespace rive

#endif
