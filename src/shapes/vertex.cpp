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
#ifdef WITH_RIVE_EDITOR
    // Editor files can carry skin-bound PointsPaths whose vertices
    // don't all have Weight children — typically a vertex authored /
    // hydrated AFTER the skin was bound, or a Weight Core that
    // failed to land in the same coop batch as its owning vertex.
    // `Skin::deform` calls into us unconditionally for every vertex
    // in the path, so skip when the weight is missing — the vertex
    // stays at its bind-space position (visually wrong but no
    // crash). Runtime importer rejects such files so the runtime
    // build never sees this case.
    if (m_Weight == nullptr)
        return;
#endif
    m_Weight->translation() = Weight::deform(Vec2D(x(), y()),
                                             m_Weight->indices(),
                                             m_Weight->values(),
                                             worldTransform,
                                             boneTransforms);
}