#include "rive/layout/n_sliced_node.hpp"
#include "rive/shapes/deformer.hpp"
#include "rive/shapes/shape.hpp"

using namespace rive;

RenderPathDeformer* RenderPathDeformer::from(Component* component)
{
    switch (component->coreType())
    {
        case NSlicedNode::typeKey:
            return component->as<NSlicedNode>();
    }
    return nullptr;
}

PointDeformer* PointDeformer::from(Component* component)
{
    switch (component->coreType())
    {
        case NSlicedNode::typeKey:
            return component->as<NSlicedNode>();
    }
    return nullptr;
}
