#ifndef _RIVE_VIEW_MODEL_INSTANCE_VIEW_MODEL_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_VIEW_MODEL_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_viewmodel_base.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/refcnt.hpp"
#include <stdio.h>
namespace rive
{
class ViewModelInstanceViewModel : public ViewModelInstanceViewModelBase
{
private:
    rcp<ViewModelInstance> m_referenceViewModelInstance;

public:
    ~ViewModelInstanceViewModel();
    void referenceViewModelInstance(rcp<ViewModelInstance> value)
    {
        m_referenceViewModelInstance = value;
    };
    rcp<ViewModelInstance> referenceViewModelInstance()
    {
        return m_referenceViewModelInstance;
    }
    void setRoot(rcp<ViewModelInstance> value) override;
    void advanced() override;
};
} // namespace rive

#endif