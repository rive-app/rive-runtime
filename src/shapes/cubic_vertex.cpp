#include "rive/shapes/cubic_vertex.hpp"
#include "rive/bones/cubic_weight.hpp"
using namespace rive;

const Vec2D& CubicVertex::renderIn()
{
    if (hasWeight())
    {
        return weight<CubicWeight>()->inTranslation();
    }
    else
    {
        return inPoint();
    }
}

const Vec2D& CubicVertex::renderOut()
{
    if (hasWeight())
    {
        return weight<CubicWeight>()->outTranslation();
    }
    else
    {
        return outPoint();
    }
}

const Vec2D& CubicVertex::inPoint()
{
    if (!m_InValid)
    {
        computeIn();
        m_InValid = true;
    }
    return m_InPoint;
}

const Vec2D& CubicVertex::outPoint()
{
    if (!m_OutValid)
    {
        computeOut();
        m_OutValid = true;
    }
    return m_OutPoint;
}

void CubicVertex::outPoint(const Vec2D& value)
{
    m_OutPoint = value;
    m_OutValid = true;
}

void CubicVertex::inPoint(const Vec2D& value)
{
    m_InPoint = value;
    m_InValid = true;
}

void CubicVertex::xChanged()
{
    Super::xChanged();
    m_InValid = m_OutValid = false;
}
void CubicVertex::yChanged()
{
    Super::yChanged();
    m_InValid = m_OutValid = false;
}

void CubicVertex::deform(const Mat2D& worldTransform,
                         const float* boneTransforms)
{
    Super::deform(worldTransform, boneTransforms);

    auto cubicWeight = weight<CubicWeight>();
#ifdef WITH_RIVE_EDITOR
    // See `Vertex::deform` — editor coop hydration can deliver a
    // skin-bound cubic vertex without its CubicWeight child (or with
    // a base Weight that isn't a CubicWeight subclass yet). Skip
    // rather than null-deref. Runtime importer rejects such files
    // so the runtime build never reaches this branch.
    if (cubicWeight == nullptr)
        return;
#endif

    cubicWeight->inTranslation() = Weight::deform(inPoint(),
                                                  cubicWeight->inIndices(),
                                                  cubicWeight->inValues(),
                                                  worldTransform,
                                                  boneTransforms);

    cubicWeight->outTranslation() = Weight::deform(outPoint(),
                                                   cubicWeight->outIndices(),
                                                   cubicWeight->outValues(),
                                                   worldTransform,
                                                   boneTransforms);
}
