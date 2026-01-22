#ifndef _RIVE_LIST_PATH_HPP_
#define _RIVE_LIST_PATH_HPP_
#include "rive/generated/shapes/list_path_base.hpp"
#include "rive/data_bind/data_bind_list_item_consumer.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/property_symbol_dependent.hpp"
#include "rive/shapes/vertex.hpp"
#include <stdio.h>
namespace rive
{

class VertexListener;

class VertexPropertyListenerSingle : public PropertySymbolDependentSingle
{
public:
    VertexPropertyListenerSingle(Core* vertex,
                                 VertexListener* vertexListener,
                                 ViewModelInstanceValue* instanceValue,
                                 uint16_t propertyKey,
                                 float multiplier);
    void writeValue() override;

protected:
    float m_multiplier = 1;
};

class VertexPropertyListenerMulti : public PropertySymbolDependentMulti
{
public:
    VertexPropertyListenerMulti(Core* vertex,
                                VertexListener* vertexListener,
                                ViewModelInstanceValue* instanceValue,
                                std::vector<uint16_t> propertyKeys,
                                float multiplier);

    void writeValue() override;

private:
    float m_multiplier = 1;
};

class VertexPropertyListenerPoint : public PropertySymbolDependent
{
public:
    ~VertexPropertyListenerPoint();
    VertexPropertyListenerPoint(Core* vertex,
                                VertexListener* vertexListener,
                                ViewModelInstanceValue* xValue,
                                ViewModelInstanceValue* yValue,
                                uint16_t distKey,
                                uint16_t rotKey);
    void writeValue() override;

private:
    ViewModelInstanceValue* m_yValue;
    uint16_t m_distKey;
    uint16_t m_rotKey;
};

class VertexListener : public CoreObjectListener
{
public:
    VertexListener(Vertex* vertex, rcp<ViewModelInstance> instance, Path* path);

    Vertex* vertex() { return m_core->as<Vertex>(); }
    void markDirty() override { m_path->markPathDirty(); }

private:
    Path* m_path = nullptr;
    void createPropertyListener(SymbolType symbolType);
    void createInPointPropertyListener();
    void createOutPointPropertyListener();
    PropertySymbolDependent* createSinglePropertyListener(
        SymbolType symbolType);
    PropertySymbolDependent* createDistancePropertyListener();
    PropertySymbolDependent* createRotationPropertyListener();

protected:
    void createProperties() override;
};

class ListPath : public ListPathBase, public DataBindListItemConsumer
{
public:
    ~ListPath();
    void updateList(std::vector<rcp<ViewModelInstanceListItem>>* list) override;

private:
    std::vector<VertexListener*> m_vertexListeners;
};
} // namespace rive

#endif