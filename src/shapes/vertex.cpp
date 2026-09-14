#include "rive/shapes/vertex.hpp"

using namespace rive;

Vec2D Vertex::renderTranslation()
{
    if (hasWeight())
    {
        return m_Weight->translation();
    }
    return Vec2D(x(), y());
}

void Vertex::xChanged() { markGeometryDirty(); }
void Vertex::yChanged() { markGeometryDirty(); }

void Vertex::deform(const Mat2D& worldTransform, const float* boneTransforms)
{
    // A skinned vertex with no Weight: in the editor, a vertex authored or
    // hydrated after the skin was bound, or a Weight that missed its coop
    // batch. Nothing at import rejects it either, so hold the bind position
    // rather than dereference null.
    if (m_Weight == nullptr)
    {
        return;
    }
    m_Weight->translation() = Weight::deform(Vec2D(x(), y()),
                                             m_Weight->indices(),
                                             m_Weight->values(),
                                             worldTransform,
                                             boneTransforms);
}