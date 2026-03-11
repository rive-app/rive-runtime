#ifndef _RIVE_VIEWMODEL_VALUE_DEPENDENT_HPP_
#define _RIVE_VIEWMODEL_VALUE_DEPENDENT_HPP_
#include "rive/dirtyable.hpp"
#include <stdio.h>
namespace rive
{
class CoreObjectListener;
class ViewModelValueDependent : public Dirtyable
{
public:
    virtual void relinkDataBind() = 0;
};

} // namespace rive

#endif