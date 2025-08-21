#ifndef _RIVE_POINTS_COMMON_PATH_HPP_
#define _RIVE_POINTS_COMMON_PATH_HPP_
#include "rive/generated/shapes/points_common_path_base.hpp"
#include <stdio.h>
namespace rive
{
class PointsCommonPath : public PointsCommonPathBase
{
public:
    bool isPathClosed() const override { return isClosed(); }

    bool isClockwise() const;
};
} // namespace rive

#endif