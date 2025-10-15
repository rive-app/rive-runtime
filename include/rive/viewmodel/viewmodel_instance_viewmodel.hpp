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
    rcp<ViewModelInstance> m_referenceViewModelInstance = nullptr;
    ViewModelInstance* m_parentViewModelInstance = nullptr;

public:
    ~ViewModelInstanceViewModel();
    void referenceViewModelInstance(rcp<ViewModelInstance> value)
    {
        if (m_referenceViewModelInstance && m_parentViewModelInstance)
        {
            m_referenceViewModelInstance->removeParent(
                m_parentViewModelInstance);
        }
        m_referenceViewModelInstance = value;
        if (m_referenceViewModelInstance && m_parentViewModelInstance)
        {
            m_referenceViewModelInstance->addParent(m_parentViewModelInstance);
        }
    };
    rcp<ViewModelInstance> referenceViewModelInstance()
    {
        return m_referenceViewModelInstance;
    }
    void parentViewModelInstance(ViewModelInstance* parent)
    {
        m_parentViewModelInstance = parent;
    }
    ViewModelInstance* parentViewModelInstance()
    {
        return m_parentViewModelInstance;
    }
    void setRoot(rcp<ViewModelInstance> value) override;
    void advanced() override;
};
} // namespace rive

#endif