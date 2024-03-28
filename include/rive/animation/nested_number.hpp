#ifndef _RIVE_NESTED_NUMBER_HPP_
#define _RIVE_NESTED_NUMBER_HPP_
#include "rive/generated/animation/nested_number_base.hpp"
#include <stdio.h>
namespace rive
{
class NestedNumber : public NestedNumberBase
{
public:
    void nestedValue(float value) override;
    float nestedValue() const override;
    void applyValue() override;

protected:
};
} // namespace rive

#endif