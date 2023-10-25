#ifndef _RIVE_NESTED_NUMBER_HPP_
#define _RIVE_NESTED_NUMBER_HPP_
#include "rive/generated/animation/nested_number_base.hpp"
#include <stdio.h>
namespace rive
{
class NestedNumber : public NestedNumberBase
{
public:
    void applyValue() override;

protected:
    void nestedValueChanged() override;
};
} // namespace rive

#endif