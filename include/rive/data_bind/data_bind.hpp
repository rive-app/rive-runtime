#ifndef _RIVE_DATA_BIND_HPP_
#define _RIVE_DATA_BIND_HPP_
#include "rive/generated/data_bind/data_bind_base.hpp"
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
    Component* target() { return m_target; };

protected:
    Component* m_target;
};
} // namespace rive

#endif