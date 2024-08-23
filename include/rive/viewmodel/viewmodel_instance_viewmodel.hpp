#ifndef _RIVE_VIEW_MODEL_INSTANCE_VIEW_MODEL_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_VIEW_MODEL_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_viewmodel_base.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include <stdio.h>
namespace rive
{
class ViewModelInstanceViewModel : public ViewModelInstanceViewModelBase
{
private:
    ViewModelInstance* m_referenceViewModelInstance;

public:
    void referenceViewModelInstance(ViewModelInstance* value)
    {
        m_referenceViewModelInstance = value;
    };
    ViewModelInstance* referenceViewModelInstance() { return m_referenceViewModelInstance; }
    void setRoot(ViewModelInstance* value) override;
};
} // namespace rive

#endif