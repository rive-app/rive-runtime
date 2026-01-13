#ifndef _RIVE_SCRIPTED_DATA_CONVERTER_HPP_
#define _RIVE_SCRIPTED_DATA_CONVERTER_HPP_
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
    DataValue* m_dataValue = nullptr;
    template <typename T = DataValue> void storeData(DataValue* input)
    {
        if (m_dataValue && !m_dataValue->is<T>())
        {
            delete m_dataValue;
        }
        if (!m_dataValue)
        {
            m_dataValue = new T();
        }
        m_dataValue->as<T>()->value(input->as<T>()->value());
    };
    virtual void disposeScriptInputs() override;
#ifdef WITH_RIVE_SCRIPTING
    DataValue* applyConversion(DataValue* value, const std::string& method);
    bool pushDataValue(DataValue*);
#endif

public:
    ~ScriptedDataConverter();
#ifdef WITH_RIVE_SCRIPTING
    bool scriptInit(lua_State* state) override;
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataValue* reverseConvert(DataValue* value, DataBind* dataBind) override;
#endif
    void bindFromContext(DataContext* dataContext, DataBind* dataBind) override;
    DataContext* dataContext() override { return m_dataContext; }
    DataType outputType() override { return DataType::any; }
    uint32_t assetId() override { return scriptAssetId(); }
    bool advanceComponent(float elapsedSeconds,
                          AdvanceFlags flags = AdvanceFlags::Animate |
                                               AdvanceFlags::NewFrame) override;
    StatusCode import(ImportStack& importStack) override;
    void addProperty(CustomProperty* prop) override;
    Core* clone() const override;
    bool addScriptedDirt(ComponentDirt value, bool recurse = false) override
    {
        markConverterDirty();
        return true;
    }
    ScriptProtocol scriptProtocol() override
    {
        return ScriptProtocol::converter;
    }
    Component* component() override { return nullptr; }
};
} // namespace rive

#endif