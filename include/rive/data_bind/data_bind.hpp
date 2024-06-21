#ifndef _RIVE_DATA_BIND_HPP_
#define _RIVE_DATA_BIND_HPP_
#include "rive/generated/data_bind/data_bind_base.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/data_bind/context/context_value.hpp"
#include <stdio.h>
namespace rive
{
class DataBind : public DataBindBase
{
public:
    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode import(ImportStack& importStack) override;
    void buildDependencies() override;
    virtual void updateSourceBinding();
    void update(ComponentDirt value) override;
    Component* target() { return m_target; };
    virtual void bind();

protected:
    Component* m_target;
    ViewModelInstanceValue* m_Source;
    std::unique_ptr<DataBindContextValue> m_ContextValue;
};
} // namespace rive

#endif