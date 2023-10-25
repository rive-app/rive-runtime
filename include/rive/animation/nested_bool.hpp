#ifndef _RIVE_NESTED_BOOL_HPP_
#define _RIVE_NESTED_BOOL_HPP_
#include "rive/generated/animation/nested_bool_base.hpp"
#include <stdio.h>
namespace rive
{
class NestedBool : public NestedBoolBase
{
public:
    void applyValue() override;

protected:
    void nestedValueChanged() override;
};
} // namespace rive

#endif