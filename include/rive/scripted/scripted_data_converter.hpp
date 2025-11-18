#ifndef _RIVE_SCRIPTED_DATA_CONVERTER_HPP_
#define _RIVE_SCRIPTED_DATA_CONVERTER_HPP_
#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif
#include "rive/generated/scripted/scripted_data_converter_base.hpp"
#include "rive/advancing_component.hpp"
#include "rive/scripted/scripted_object.hpp"
#include <stdio.h>
namespace rive
{
class DataBind;
class DataContext;
class DataValue;

class ScriptedDataConverter : public ScriptedDataConverterBase,
                              public ScriptedObject,
                              public AdvancingComponent
{
private:
    DataContext* m_dataContext = nullptr;

public:
#ifdef WITH_RIVE_SCRIPTING
    bool scriptInit(LuaState* state) override;
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
#endif
    void bindFromContext(DataContext* dataContext, DataBind* dataBind) override;
    DataContext* dataContext() override { return m_dataContext; }
    DataType outputType() override { return DataType::any; }
    uint32_t assetId() override { return scriptAssetId(); }
    bool advanceComponent(float elapsedSeconds,
                          AdvanceFlags flags = AdvanceFlags::Animate |
                                               AdvanceFlags::NewFrame) override;
    StatusCode import(ImportStack& importStack) override;
    Core* clone() const override;
    bool addScriptedDirt(ComponentDirt value, bool recurse = false) override
    {
        markConverterDirty();
        return true;
    }
    ScriptType scriptType() override { return ScriptType::converter; }
    Component* component() override { return nullptr; }
};
} // namespace rive

#endif