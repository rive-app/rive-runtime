#ifndef _RIVE_STAR_HPP_
#define _RIVE_STAR_HPP_
#include "rive/generated/shapes/star_base.hpp"
#include <stdio.h>
namespace rive
{
class Star : public StarBase
{
public:
    Star();
    void update(ComponentDirt value) override;

protected:
    void innerRadiusChanged() override;
    std::size_t vertexCount() override;
    void buildPolygon() override;
};
} // namespace rive

#endif