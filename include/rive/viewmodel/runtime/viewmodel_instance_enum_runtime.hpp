#ifndef _RIVE_VIEW_MODEL_INSTANCE_ENUM_RUNTIME_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_ENUM_RUNTIME_HPP_

#include <string>
#include <stdint.h>
#include "rive/viewmodel/runtime/viewmodel_instance_value_runtime.hpp"
#include "rive/viewmodel/viewmodel_instance_enum.hpp"
#include "rive/viewmodel/data_enum.hpp"
#include "rive/viewmodel/data_enum_value.hpp"

namespace rive
{

class ViewModelInstanceEnumRuntime : public ViewModelInstanceValueRuntime
{

public:
    ViewModelInstanceEnumRuntime(ViewModelInstanceEnum* viewModelInstance) :
        ViewModelInstanceValueRuntime(viewModelInstance)
    {}
    std::string value() const;
    void value(std::string);
    uint32_t valueIndex() const;
    void valueIndex(uint32_t);
    std::vector<std::string> values() const;

private:
    std::vector<DataEnumValue*> dataValues() const;
};
} // namespace rive
#endif
