#ifndef _RIVE_PROPERTY_SYMBOl_DEPENDENT_HPP_
#define _RIVE_PROPERTY_SYMBOl_DEPENDENT_HPP_
#include "rive/dirtyable.hpp"
#include "rive/core.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include <stdio.h>
namespace rive
{
class CoreObjectListener;
class PropertySymbolDependent : public Dirtyable
{
public:
    PropertySymbolDependent(Core* coreObject,
                            CoreObjectListener* coreObjectListener,
                            ViewModelInstanceValue* instanceValue);
    virtual ~PropertySymbolDependent();

    void addDirt(ComponentDirt value, bool recurse) override;
    virtual void writeValue() = 0;

protected:
    Core* m_coreObject = nullptr;
    CoreObjectListener* m_coreObjectListener = nullptr;
    ViewModelInstanceValue* m_instanceValue = nullptr;
};

class PropertySymbolDependentSingle : public PropertySymbolDependent
{
public:
    PropertySymbolDependentSingle(Core* vertex,
                                  CoreObjectListener* vertexListener,
                                  ViewModelInstanceValue* instanceValue,
                                  uint16_t propertyKey);

protected:
    uint16_t m_propertyKey = 0;
};

class PropertySymbolDependentMulti : public PropertySymbolDependent
{
public:
    PropertySymbolDependentMulti(Core* vertex,
                                 CoreObjectListener* vertexListener,
                                 ViewModelInstanceValue* instanceValue,
                                 std::vector<uint16_t> propertyKeys);

protected:
    std::vector<uint16_t> m_propertyKeys;
};

class CoreObjectListener
{
public:
    CoreObjectListener(Core* core, rcp<ViewModelInstance> instance);
    virtual ~CoreObjectListener();
    void remap(rcp<ViewModelInstance> instance);
    virtual void markDirty() {}

protected:
    Core* m_core = nullptr;
    rcp<ViewModelInstance> m_instance = nullptr;
    virtual void createProperties();
    virtual void deleteProperties();
    std::vector<PropertySymbolDependent*> m_properties;
};
} // namespace rive

#endif