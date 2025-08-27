#ifndef _RIVE_VIEW_MODEL_INSTANCE_VALUE_RUNTIME_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_VALUE_RUNTIME_HPP_

#include <string>
#include <stdint.h>
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/dirtyable.hpp"
#include "rive/data_bind/data_values/data_type.hpp"

namespace rive
{

class ViewModelInstanceValueRuntime : public Dirtyable
{

public:
    ViewModelInstanceValueRuntime(ViewModelInstanceValue* instanceValue);
    virtual ~ViewModelInstanceValueRuntime();
    virtual const DataType dataType() = 0;
    void addDirt(ComponentDirt dirt, bool recurse) override;
    void clearChanges();
    bool hasChanged() const { return m_hasChanged; }
    bool flushChanges();
    const std::string& name() const;
    ViewModelInstanceValue* viewModelInstanceValue()
    {
        return m_viewModelInstanceValue;
    }

protected:
    ViewModelInstanceValue* m_viewModelInstanceValue = nullptr;

private:
    bool m_hasChanged = false;
};
} // namespace rive
#endif
