#include "rive/viewmodel/property_symbol_dependent.hpp"

using namespace rive;

PropertySymbolDependent::PropertySymbolDependent(
    Core* core,
    CoreObjectListener* coreObjectListener,
    ViewModelInstanceValue* instanceValue) :
    m_coreObject(core),
    m_coreObjectListener(coreObjectListener),
    m_instanceValue(instanceValue)
{
    if (m_instanceValue != nullptr)
    {
        m_instanceValue->addDependent(this);
    }
}

PropertySymbolDependent::~PropertySymbolDependent()
{

    if (m_instanceValue != nullptr)
    {
        m_instanceValue->removeDependent(this);
    }
}

void PropertySymbolDependent::addDirt(ComponentDirt value, bool recurse)
{
    writeValue();
    m_coreObjectListener->markDirty();
}

PropertySymbolDependentSingle::PropertySymbolDependentSingle(
    Core* core,
    CoreObjectListener* coreObjectListener,
    ViewModelInstanceValue* instanceValue,
    uint16_t propertyKey) :
    PropertySymbolDependent(core, coreObjectListener, instanceValue),
    m_propertyKey(propertyKey)
{}

PropertySymbolDependentMulti::PropertySymbolDependentMulti(
    Core* core,
    CoreObjectListener* coreObjectListener,
    ViewModelInstanceValue* instanceValue,
    std::vector<uint16_t> propertyKeys) :
    PropertySymbolDependent(core, coreObjectListener, instanceValue),
    m_propertyKeys(propertyKeys)
{}

CoreObjectListener::CoreObjectListener(Core* core,
                                       rcp<ViewModelInstance> instance) :
    m_core(core), m_instance(instance)
{}

CoreObjectListener::~CoreObjectListener()
{
    delete m_core;
    deleteProperties();
}

void CoreObjectListener::createProperties() { deleteProperties(); }

void CoreObjectListener::deleteProperties()
{
    for (auto& property : m_properties)
    {
        delete property;
    }
    m_properties.clear();
}

void CoreObjectListener::remap(rcp<ViewModelInstance> instance)
{
    if (instance != m_instance)
    {
        m_instance = instance;
        createProperties();
    }
}