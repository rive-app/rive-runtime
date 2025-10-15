#ifndef _RIVE_SCRIPT_INPUT_VIEW_MODEL_PROPERTY_HPP_
#define _RIVE_SCRIPT_INPUT_VIEW_MODEL_PROPERTY_HPP_
#include "rive/generated/script_input_viewmodel_property_base.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/scripted/scripted_object.hpp"
#include <stdio.h>
namespace rive
{
class ViewModelInstanceValue;

class ScriptInputViewModelProperty : public ScriptInputViewModelPropertyBase,
                                     public ScriptInput
{
private:
    ViewModelInstanceValue* m_viewModelInstanceValue;

protected:
    std::vector<uint32_t> m_DataBindPathIdsBuffer;

public:
    ScriptedObject* scriptedObject() { return ScriptedObject::from(parent()); }
    void decodeDataBindPathIds(Span<const uint8_t> value) override;
    void copyDataBindPathIds(
        const ScriptInputViewModelPropertyBase& object) override;
    std::vector<uint32_t> dataBindPathIds() { return m_DataBindPathIdsBuffer; };
    void initScriptedValue() override;
    bool validateForScriptInit() override;
};
} // namespace rive

#endif