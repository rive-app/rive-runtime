#ifndef _RIVE_LIST_PATH_HPP_
#define _RIVE_LIST_PATH_HPP_
#include "rive/generated/shapes/list_path_base.hpp"
#include "rive/data_bind/data_bind_list_item_consumer.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/shapes/vertex.hpp"
#include <stdio.h>
namespace rive
{

class VertexListener;

class VertexPropertyListenerBase : public Dirtyable
{
public:
    VertexPropertyListenerBase(Vertex* vertex, VertexListener* vertexListener);
    virtual ~VertexPropertyListenerBase() = default;

    void addDirt(ComponentDirt value, bool recurse) override;
    virtual void writeValue() = 0;

protected:
    Vertex* m_vertex = nullptr;
    VertexListener* m_vertexListener = nullptr;
};

class VertexPropertyListener : public VertexPropertyListenerBase
{
public:
    VertexPropertyListener(Vertex* vertex,
                           VertexListener* vertexListener,
                           ViewModelInstanceValue* instanceValue,
                           float multiplier);
    virtual ~VertexPropertyListener();

protected:
    ViewModelInstanceValue* m_instanceValue = nullptr;
    float m_multiplier = 1;
};

class VertexPropertyListenerSingle : public VertexPropertyListener
{
public:
    VertexPropertyListenerSingle(Vertex* vertex,
                                 VertexListener* vertexListener,
                                 ViewModelInstanceValue* instanceValue,
                                 float multiplier,
                                 uint16_t propertyKey);
    ~VertexPropertyListenerSingle();
    void writeValue() override;

protected:
    uint16_t m_propertyKey = 0;
};

class VertexPropertyListenerMulti : public VertexPropertyListener
{
public:
    VertexPropertyListenerMulti(Vertex* vertex,
                                VertexListener* vertexListener,
                                ViewModelInstanceValue* instanceValue,
                                float multiplier,
                                std::vector<uint16_t> propertyKeys);

    void writeValue() override;

private:
    std::vector<uint16_t> m_propertyKeys;
};

class VertexPropertyListenerPoint : public VertexPropertyListenerBase
{
public:
    ~VertexPropertyListenerPoint();
    VertexPropertyListenerPoint(Vertex* vertex,
                                VertexListener* vertexListener,
                                ViewModelInstanceValue* xValue,
                                ViewModelInstanceValue* yValue,
                                uint16_t distKey,
                                uint16_t rotKey);
    void writeValue() override;

private:
    ViewModelInstanceValue* m_xValue;
    ViewModelInstanceValue* m_yValue;
    uint16_t m_distKey;
    uint16_t m_rotKey;
};

class VertexListener
{
public:
    VertexListener(Vertex* vertex, rcp<ViewModelInstance> instance, Path* path);
    ~VertexListener();

    Vertex* vertex() { return m_vertex; }
    void markDirty() { m_path->markPathDirty(); }
    void remap(rcp<ViewModelInstance> instance);

private:
    Vertex* m_vertex = nullptr;
    rcp<ViewModelInstance> m_instance = nullptr;
    Path* m_path = nullptr;
    std::vector<VertexPropertyListenerBase*> m_properties;
    void createPropertyListener(SymbolType symbolType);
    void createInPointPropertyListener();
    void createOutPointPropertyListener();
    VertexPropertyListenerBase* createSinglePropertyListener(
        SymbolType symbolType);
    VertexPropertyListenerBase* createDistancePropertyListener();
    VertexPropertyListenerBase* createRotationPropertyListener();
    void createProperties();
    void deleteProperties();
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