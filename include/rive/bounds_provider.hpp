#ifndef _RIVE_BOUNDS_PROVIDER_HPP_
#define _RIVE_BOUNDS_PROVIDER_HPP_

#include "rive/math/aabb.hpp"
#include "rive/math/mat2d.hpp"

namespace rive
{

class BoundsProvider
{
public:
    virtual ~BoundsProvider() {}
    virtual AABB computeBounds(Mat2D toParent);
};
} // namespace rive
#endif