#ifndef _RIVE_CUBIC_WEIGHT_HPP_
#define _RIVE_CUBIC_WEIGHT_HPP_
#include "rive/generated/bones/cubic_weight_base.hpp"
#include <stdio.h>
namespace rive
{
class CubicWeight : public CubicWeightBase
{
private:
    Vec2D m_InTranslation;
    Vec2D m_OutTranslation;

public:
    Vec2D& inTranslation() { return m_InTranslation; }
    Vec2D& outTranslation() { return m_OutTranslation; }
};
} // namespace rive

#endif