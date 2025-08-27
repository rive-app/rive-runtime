#ifndef _RIVE_DATA_BIND_HPP_
#define _RIVE_DATA_BIND_HPP_
#include "rive/component_dirt.hpp"
#include "rive/generated/data_bind/data_bind_base.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
#include "rive/data_bind/data_values/data_type.hpp"
#include "rive/dirtyable.hpp"
#include <stdio.h>
namespace rive
{
class File;
class DataBindContextValue;
#ifdef WITH_RIVE_TOOLS
class DataBind;
typedef void (*DataBindChanged)();
#endif
class DataBind : public DataBindBase, public Dirtyable
{
public:
    ~DataBind();
    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode import(ImportStack& importStack) override;
    virtual void updateSourceBinding(bool invalidate = false);
    virtual void update(ComponentDirt value);
    Core* target() const { return m_target; };
    void target(Core* value) { m_target = value; };
    virtual void bind();
    virtual void unbind();
    ComponentDirt dirt() { return m_Dirt; };
    void dirt(ComponentDirt value) { m_Dirt = value; };
    void addDirt(ComponentDirt value, bool recurse) override;
    DataConverter* converter() const { return m_dataConverter; };
    void converter(DataConverter* value) { m_dataConverter = value; };
    ViewModelInstanceValue* source() const { return m_Source; };
    void source(ViewModelInstanceValue* value);
    void clearSource();
    bool toSource();
    bool toTarget();
    bool advance(float elapsedTime);
    void suppressDirt(bool value) { m_suppressDirt = value; };
    void file(File* value) { m_file = value; };
    File* file() const { return m_file; };

protected:
    ComponentDirt m_Dirt = ComponentDirt::Filthy;
    Core* m_target = nullptr;
    ViewModelInstanceValue* m_Source = nullptr;
    DataBindContextValue* m_ContextValue = nullptr;
    DataConverter* m_dataConverter = nullptr;
    DataType outputType();
    bool bindsOnce();
    bool m_suppressDirt = false;
    File* m_file;
#ifdef WITH_RIVE_TOOLS
public:
    void onChanged(DataBindChanged callback) { m_changedCallback = callback; }
    DataBindChanged m_changedCallback = nullptr;
#endif
};
} // namespace rive

#endif