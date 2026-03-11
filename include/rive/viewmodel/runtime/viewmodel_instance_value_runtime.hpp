#ifndef _RIVE_VIEW_MODEL_INSTANCE_VALUE_RUNTIME_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_VALUE_RUNTIME_HPP_

#include <string>
#include <stdint.h>
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/viewmodel/viewmodel_value_dependent.hpp"
#include "rive/data_bind/data_values/data_type.hpp"

namespace rive
{

class ViewModelInstanceValueRuntime : public ViewModelValueDependent
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
    void relinkDataBind() override;

protected:
    ViewModelInstanceValue* m_viewModelInstanceValue = nullptr;

private:
    bool m_hasChanged = false;
};
} // namespace rive
#endif
