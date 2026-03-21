#ifndef _RIVE_VIEW_MODEL_INSTANCE_VIEW_MODEL_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_VIEW_MODEL_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_viewmodel_base.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/refcnt.hpp"
#include "rive/data_bind/data_bind_viewmodel_consumer.hpp"
#include <stdio.h>
namespace rive
{
class DataValueViewModel;
#ifdef WITH_RIVE_TOOLS
class ViewModelInstanceViewModel;
typedef void (*ViewModelViewModelChanged)(ViewModelInstanceViewModel* vmi);
#endif
class ViewModelInstanceViewModel : public ViewModelInstanceViewModelBase,
                                   public DataBindViewModelConsumer
{
private:
    rcp<ViewModelInstance> m_referenceViewModelInstance = nullptr;
    ViewModelInstance* m_parentViewModelInstance = nullptr;

public:
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
        propertyValueChanged();
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
    void updateViewModel(ViewModelInstance*) override;
    void applyValue(DataValueViewModel*);
    StatusCode import(ImportStack& importStack) override;
    Core* clone() const override;
#ifdef WITH_RIVE_TOOLS
    void onChanged(ViewModelViewModelChanged callback)
    {
        m_changedCallback = callback;
    }
    ViewModelViewModelChanged m_changedCallback = nullptr;
#endif

protected:
    void propertyValueChanged() override;
};
} // namespace rive

#endif