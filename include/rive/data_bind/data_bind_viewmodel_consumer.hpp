#ifndef _RIVE_DATA_VIEWMODEL_CONSUMER_HPP_
#define _RIVE_DATA_VIEWMODEL_CONSUMER_HPP_
#include "rive/core.hpp"
namespace rive
{
class ViewModelInstance;

class DataBindViewModelConsumer
{
public:
    static DataBindViewModelConsumer* from(Core* component);
    virtual void updateViewModel(ViewModelInstance* value) = 0;
};
} // namespace rive

#endif