#include "rive/shapes/list_path.hpp"
#include "rive/math/math_types.hpp"
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

VertexListener::VertexListener(Vertex* vertex,
                               rcp<ViewModelInstance> instance,
                               Path* path) :
    CoreObjectListener(vertex, instance), m_path(path)
{
    createProperties();
}

void VertexListener::createProperties()
{
    CoreObjectListener::createProperties();
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

PropertySymbolDependent* VertexListener::createSinglePropertyListener(
    SymbolType symbolType)
{
    uint16_t propertyKey = 0;
    float multiplier = 1.0f;
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
            multiplier = math::PI / 180.0f;
            break;
        case SymbolType::outRotation:
            propertyKey = CubicDetachedVertexBase::outRotationPropertyKey;
            multiplier = math::PI / 180.0f;
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
        auto vpl = new VertexPropertyListenerSingle(m_core,
                                                    this,
                                                    prop,
                                                    propertyKey,
                                                    multiplier);
        return vpl;
    }
    return nullptr;
}

PropertySymbolDependent* VertexListener::createDistancePropertyListener()
{

    auto prop = m_instance->propertyValue(SymbolType::distance);
    if (prop != nullptr && prop->is<ViewModelInstanceNumber>())
    {
        std::vector<uint16_t> keys = {};
        auto inPropKey = CubicDetachedVertexBase::inDistancePropertyKey;
        keys.push_back(inPropKey);
        auto outPropKey = CubicDetachedVertexBase::outDistancePropertyKey;
        keys.push_back(outPropKey);
        auto vpl =
            new VertexPropertyListenerMulti(m_core, this, prop, keys, 1.0f);
        return vpl;
    }
    return nullptr;
}

PropertySymbolDependent* VertexListener::createRotationPropertyListener()
{

    auto prop = m_instance->propertyValue(SymbolType::rotation);
    if (prop != nullptr && prop->is<ViewModelInstanceNumber>())
    {
        std::vector<uint16_t> keys = {};
        auto inPropKey = CubicDetachedVertexBase::inRotationPropertyKey;
        keys.push_back(inPropKey);
        auto outPropKey = CubicDetachedVertexBase::outRotationPropertyKey;
        keys.push_back(outPropKey);
        auto vpl = new VertexPropertyListenerMulti(m_core,
                                                   this,
                                                   prop,
                                                   keys,
                                                   math::PI / 180.0f);
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
            m_core,
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
            m_core,
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
    PropertySymbolDependent* listener = nullptr;
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

VertexPropertyListenerSingle::VertexPropertyListenerSingle(
    Core* vertex,
    VertexListener* vertexListener,
    ViewModelInstanceValue* instanceValue,
    uint16_t propertyKey,
    float multiplier) :
    PropertySymbolDependentSingle(vertex,
                                  vertexListener,
                                  instanceValue,
                                  propertyKey),
    m_multiplier(multiplier)
{}

void VertexPropertyListenerSingle::writeValue()
{
    CoreRegistry::setDouble(
        m_coreObject,
        m_propertyKey,
        m_instanceValue->as<ViewModelInstanceNumber>()->propertyValue() *
            m_multiplier);
}

VertexPropertyListenerMulti::VertexPropertyListenerMulti(
    Core* vertex,
    VertexListener* vertexListener,
    ViewModelInstanceValue* instanceValue,
    std::vector<uint16_t> propertyKeys,
    float multiplier) :
    PropertySymbolDependentMulti(vertex,
                                 vertexListener,
                                 instanceValue,
                                 propertyKeys),
    m_multiplier(multiplier)
{}

void VertexPropertyListenerMulti::writeValue()
{
    for (auto& key : m_propertyKeys)
    {
        CoreRegistry::setDouble(
            m_coreObject,
            key,
            m_instanceValue->as<ViewModelInstanceNumber>()->propertyValue() *
                m_multiplier);
    }
}

VertexPropertyListenerPoint::VertexPropertyListenerPoint(
    Core* vertex,
    VertexListener* vertexListener,
    ViewModelInstanceValue* xValue,
    ViewModelInstanceValue* yValue,
    uint16_t distKey,
    uint16_t rotKey) :
    PropertySymbolDependent(vertex, vertexListener, xValue),
    m_yValue(yValue),
    m_distKey(distKey),
    m_rotKey(rotKey)
{
    if (m_yValue)
    {
        m_yValue->addDependent(this);
    }
}

VertexPropertyListenerPoint::~VertexPropertyListenerPoint()
{
    if (m_yValue)
    {
        m_yValue->removeDependent(this);
    }
}

void VertexPropertyListenerPoint::writeValue()
{
    auto x =
        m_instanceValue
            ? m_instanceValue->as<ViewModelInstanceNumber>()->propertyValue()
            : 0;
    auto y =
        m_yValue ? m_yValue->as<ViewModelInstanceNumber>()->propertyValue() : 0;
    auto vec = Vec2D(x, y);
    auto length = vec.length();
    auto rotation = std::atan2(vec.y, vec.x);
    CoreRegistry::setDouble(m_coreObject, m_distKey, length);
    CoreRegistry::setDouble(m_coreObject, m_rotKey, rotation);
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