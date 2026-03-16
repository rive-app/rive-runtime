#ifndef _RIVE_LISTENER_INPUT_TYPE_HPP_
#define _RIVE_LISTENER_INPUT_TYPE_HPP_
#include "rive/generated/animation/listener_types/listener_input_type_base.hpp"
#include <stdio.h>
namespace rive
{
class ListenerInputType : public ListenerInputTypeBase
{
public:
    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif