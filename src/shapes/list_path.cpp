#include "rive/shapes/list_path.hpp"
#include "rive/artboard.hpp"
#include "rive/shapes/cubic_detached_vertex.hpp"
#include "rive/shapes/straight_vertex.hpp"
#include "rive/shapes/vertex.hpp"
#include "rive/shapes/path_vertex.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/symbol_type.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

VertexListener::~VertexListener()
{
    delete m_vertex;
    deleteProperties();
}

void VertexListener::deleteProperties()
{

    for (auto& property : m_properties)
    {
        delete property;
    }
    m_properties.clear();
}

VertexListener::VertexListener(Vertex* vertex,
                               rcp<ViewModelInstance> instance,
                               Path* path) :
    m_vertex(vertex), m_instance(instance), m_path(path)
{
    createProperties();
}

void VertexListener::createProperties()
{

    createPropertyListener(SymbolType::vertexX);
    createPropertyListener(SymbolType::vertexY);
    createPropertyListener(SymbolType::rotation);
    createPropertyListener(SymbolType::inRotation);
    createPropertyListener(SymbolType::outRotation);
    createPropertyListener(SymbolType::distance);
    createPropertyListener(SymbolType::inDistance);
    createPropertyListener(SymbolType::outDistance);
    createInPointPropertyListener();
    createOutPointPropertyListener();
}

void VertexListener::remap(rcp<ViewModelInstance> instance)
{
    if (instance != m_instance)
    {
        m_instance = instance;
        deleteProperties();
        createProperties();
    }
}

VertexPropertyListenerBase* VertexListener::createSinglePropertyListener(
    SymbolType symbolType)
{
    uint16_t propertyKey = 0;
    switch (symbolType)
    {
        case SymbolType::vertexX:
            propertyKey = VertexBase::xPropertyKey;
            break;
        case SymbolType::vertexY:
            propertyKey = VertexBase::yPropertyKey;
            break;
        case SymbolType::inRotation:
            propertyKey = CubicDetachedVertexBase::inRotationPropertyKey;
            break;
        case SymbolType::outRotation:
            propertyKey = CubicDetachedVertexBase::outRotationPropertyKey;
            break;
        case SymbolType::inDistance:
            propertyKey = CubicDetachedVertexBase::inDistancePropertyKey;
            break;
        case SymbolType::outDistance:
            propertyKey = CubicDetachedVertexBase::outDistancePropertyKey;
            break;
        default:
            break;
    }

    auto prop = m_instance->propertyValue(symbolType);
    if (prop != nullptr && prop->is<ViewModelInstanceNumber>())
    {
        auto vpl =
            new VertexPropertyListenerSingle(m_vertex, this, prop, propertyKey);
        return vpl;
    }
    return nullptr;
}

VertexPropertyListenerBase* VertexListener::createDistancePropertyListener()
{

    auto prop = m_instance->propertyValue(SymbolType::distance);
    if (prop != nullptr && prop->is<ViewModelInstanceNumber>())
    {
        std::vector<uint16_t> keys = {};
        auto inPropKey = CubicDetachedVertexBase::inDistancePropertyKey;
        keys.push_back(inPropKey);
        auto outPropKey = CubicDetachedVertexBase::outDistancePropertyKey;
        keys.push_back(outPropKey);
        auto vpl = new VertexPropertyListenerMulti(m_vertex, this, prop, keys);
        return vpl;
    }
    return nullptr;
}

VertexPropertyListenerBase* VertexListener::createRotationPropertyListener()
{

    auto prop = m_instance->propertyValue(SymbolType::distance);
    if (prop != nullptr && prop->is<ViewModelInstanceNumber>())
    {
        std::vector<uint16_t> keys = {};
        auto inPropKey = CubicDetachedVertexBase::inRotationPropertyKey;
        keys.push_back(inPropKey);
        auto outPropKey = CubicDetachedVertexBase::outRotationPropertyKey;
        keys.push_back(outPropKey);
        auto vpl = new VertexPropertyListenerMulti(m_vertex, this, prop, keys);
        return vpl;
    }
    return nullptr;
}

void VertexListener::createInPointPropertyListener()
{
    auto xProp = m_instance->propertyValue(SymbolType::cubicVertexInPointX);
    auto yProp = m_instance->propertyValue(SymbolType::cubicVertexInPointY);
    if ((xProp != nullptr && xProp->is<ViewModelInstanceNumber>()) ||
        (yProp != nullptr && yProp->is<ViewModelInstanceNumber>()))
    {
        auto vpl = new VertexPropertyListenerPoint(
            m_vertex,
            this,
            xProp,
            yProp,
            CubicDetachedVertexBase::inDistancePropertyKey,
            CubicDetachedVertexBase::inRotationPropertyKey);

        vpl->writeValue();
        m_properties.push_back(vpl);
    }
}

void VertexListener::createOutPointPropertyListener()
{
    auto xProp = m_instance->propertyValue(SymbolType::cubicVertexOutPointX);
    auto yProp = m_instance->propertyValue(SymbolType::cubicVertexOutPointY);
    if ((xProp != nullptr && xProp->is<ViewModelInstanceNumber>()) ||
        (yProp != nullptr && yProp->is<ViewModelInstanceNumber>()))
    {
        auto vpl = new VertexPropertyListenerPoint(
            m_vertex,
            this,
            xProp,
            yProp,
            CubicDetachedVertexBase::outDistancePropertyKey,
            CubicDetachedVertexBase::outRotationPropertyKey);

        vpl->writeValue();
        m_properties.push_back(vpl);
    }
}

void VertexListener::createPropertyListener(SymbolType symbolType)
{
    VertexPropertyListenerBase* listener = nullptr;
    switch (symbolType)
    {
        case SymbolType::vertexX:
        case SymbolType::vertexY:
        case SymbolType::inRotation:
        case SymbolType::outRotation:
        case SymbolType::inDistance:
        case SymbolType::outDistance:
            listener = createSinglePropertyListener(symbolType);
            break;
        case SymbolType::distance:
            listener = createDistancePropertyListener();
            break;
        case SymbolType::rotation:
            listener = createRotationPropertyListener();
            break;
        default:
            break;
    }
    if (listener != nullptr)
    {
        listener->writeValue();
        m_properties.push_back(listener);
    }
}

VertexPropertyListenerBase::VertexPropertyListenerBase(
    Vertex* vertex,
    VertexListener* vertexListener) :
    m_vertex(vertex), m_vertexListener(vertexListener)
{}

void VertexPropertyListenerBase::addDirt(ComponentDirt value, bool recurse)
{
    writeValue();
    m_vertexListener->markDirty();
}

VertexPropertyListener::VertexPropertyListener(
    Vertex* vertex,
    VertexListener* vertexListener,
    ViewModelInstanceValue* instanceValue) :
    VertexPropertyListenerBase(vertex, vertexListener),
    m_instanceValue(instanceValue)
{

    if (m_instanceValue != nullptr)
    {
        m_instanceValue->addDependent(this);
    }
}

VertexPropertyListener::~VertexPropertyListener()
{

    if (m_instanceValue != nullptr)
    {
        m_instanceValue->removeDependent(this);
    }
}

VertexPropertyListenerSingle::VertexPropertyListenerSingle(
    Vertex* vertex,
    VertexListener* vertexListener,
    ViewModelInstanceValue* instanceValue,
    uint16_t propertyKey) :
    VertexPropertyListener(vertex, vertexListener, instanceValue),
    m_propertyKey(propertyKey)
{}

VertexPropertyListenerSingle::~VertexPropertyListenerSingle() {}

void VertexPropertyListenerSingle::writeValue()
{
    CoreRegistry::setDouble(
        m_vertex,
        m_propertyKey,
        m_instanceValue->as<ViewModelInstanceNumber>()->propertyValue());
}

VertexPropertyListenerMulti::VertexPropertyListenerMulti(
    Vertex* vertex,
    VertexListener* vertexListener,
    ViewModelInstanceValue* instanceValue,
    std::vector<uint16_t> propertyKeys) :
    VertexPropertyListener(vertex, vertexListener, instanceValue),
    m_propertyKeys(propertyKeys)
{}

void VertexPropertyListenerMulti::writeValue()
{
    for (auto& key : m_propertyKeys)
    {

        CoreRegistry::setDouble(
            m_vertex,
            key,
            m_instanceValue->as<ViewModelInstanceNumber>()->propertyValue());
    }
}

VertexPropertyListenerPoint::VertexPropertyListenerPoint(
    Vertex* vertex,
    VertexListener* vertexListener,
    ViewModelInstanceValue* xValue,
    ViewModelInstanceValue* yValue,
    uint16_t distKey,
    uint16_t rotKey) :
    VertexPropertyListenerBase(vertex, vertexListener),
    m_xValue(xValue),
    m_yValue(yValue),
    m_distKey(distKey),
    m_rotKey(rotKey)
{
    if (m_xValue)
    {
        m_xValue->addDependent(this);
    }
    if (m_yValue)
    {
        m_yValue->addDependent(this);
    }
}

VertexPropertyListenerPoint::~VertexPropertyListenerPoint()
{

    if (m_xValue)
    {
        m_xValue->removeDependent(this);
    }
    if (m_yValue)
    {
        m_yValue->removeDependent(this);
    }
}

void VertexPropertyListenerPoint::writeValue()
{
    auto x =
        m_xValue ? m_xValue->as<ViewModelInstanceNumber>()->propertyValue() : 0;
    auto y =
        m_yValue ? m_yValue->as<ViewModelInstanceNumber>()->propertyValue() : 0;
    auto vec = Vec2D(x, y);
    auto length = vec.length();
    auto rotation = std::atan2(vec.y, vec.x);
    CoreRegistry::setDouble(m_vertex, m_distKey, length);
    CoreRegistry::setDouble(m_vertex, m_rotKey, rotation);
}

ListPath::~ListPath()
{
    for (auto& vertex : m_vertexListeners)
    {
        delete vertex;
    }
}

void ListPath::updateList(std::vector<rcp<ViewModelInstanceListItem>>* list)
{
    auto currentVertexSize = m_vertexListeners.size();
    size_t index = 0;
    for (auto& listItem : *list)
    {
        auto instance = listItem->viewModelInstance();
        if (instance != nullptr)
        {
            if (index >= currentVertexSize)
            {
                // We treat all vertices as CubicDetached vertices because all
                // others can be expressed by this one. There doesn't seem to be
                // significant performance implications of doing so. But in the
                // future it might be valuable to be smarter and build the
                // corresponding one.
                auto vertex = new CubicDetachedVertex();
                auto vertexListener =
                    new VertexListener(vertex, instance, this);
                m_vertexListeners.push_back(vertexListener);
                addVertex(vertex);
            }
            else
            {
                auto vertexListener = m_vertexListeners[index];
                vertexListener->remap(instance);
            }
            index++;
        }
    }
    while (m_vertexListeners.size() > index)
    {
        auto vertex = m_vertexListeners.back();
        m_vertexListeners.pop_back();
        popVertex();
        delete vertex;
    }
    markPathDirty();
}