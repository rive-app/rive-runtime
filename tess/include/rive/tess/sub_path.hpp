#ifndef _RIVE_SUB_PATH_HPP_
#define _RIVE_SUB_PATH_HPP_

#include "rive/renderer.hpp"
#include "rive/math/mat2d.hpp"

namespace rive
{
///
/// A reference to a sub-path added to a TessRenderPath with its relative
/// transform.
///
class SubPath
{
private:
    RenderPath* m_Path;
    Mat2D m_Transform;

public:
    SubPath(RenderPath* path, const Mat2D& transform);

    RenderPath* path() const;
    const Mat2D& transform() const;
};
} // namespace rive
#endif