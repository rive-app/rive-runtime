#ifndef _RIVE_SHAPE_PAINT_PATH_HPP_
#define _RIVE_SHAPE_PAINT_PATH_HPP_

#include "rive/math/raw_path.hpp"
#include "rive/renderer.hpp"

namespace rive
{
class Component;
class Factory;
class ShapePaintPath
{
public:
    ShapePaintPath(bool isLocal = true);
    ShapePaintPath(bool isLocal, FillRule fillRule);
    RenderPath* renderPath(const Component* component);
    RenderPath* renderPath(Factory* factory);
    const RawPath* rawPath() const { return &m_rawPath; }
    RawPath* mutableRawPath() { return &m_rawPath; }
    bool isLocal() const { return m_isLocal; }
    FillRule fillRule() const { return m_fillRule; }
    bool empty() const { return m_rawPath.empty(); }

    void rewind();
    void rewind(bool isLocal, FillRule fillRule)
    {
        m_isLocal = isLocal;
        m_fillRule = fillRule;
        rewind();
    }
    void rewind(bool isLocal)
    {
        m_isLocal = isLocal;
        rewind();
    }
    void addPath(const RawPath& rawPath, const Mat2D* transform = nullptr);
    void addPathBackwards(const RawPath& rawPath,
                          const Mat2D* transform = nullptr);

    // Determines winding of the rawPath and adds in the clockwise order.
    void addPathClockwise(const RawPath& rawPath,
                          const Mat2D* transform = nullptr);

    void addPath(const ShapePaintPath* path, const Mat2D* transform = nullptr)
    {
        return addPath(*path->rawPath(), transform);
    }
    void addPathBackwards(const ShapePaintPath* path,
                          const Mat2D* transform = nullptr)
    {
        return addPathBackwards(*path->rawPath(), transform);
    }

    void addRect(const AABB& aabb, PathDirection dir = PathDirection::cw)
    {
        m_rawPath.addRect(aabb, dir);
    }

    const bool hasRenderPath() const
    {
        return m_renderPath != nullptr && !m_isRenderPathDirty;
    }

#ifdef TESTING
    size_t numContours()
    {
        size_t contours = 0;
        for (auto verb : m_rawPath.verbs())
        {
            if (verb == PathVerb::move)
            {
                contours++;
            }
        }
        return contours;
    }
#endif
private:
    bool m_isRenderPathDirty = true;
    rcp<RenderPath> m_renderPath;
    RawPath m_rawPath;
    bool m_isLocal;
    FillRule m_fillRule = FillRule::clockwise;
};
} // namespace rive
#endif