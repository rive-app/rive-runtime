#include "rive/artboard_component_list.hpp"
#include "rive/component.hpp"
#include "rive/constraints/list_constraint.hpp"
#include "rive/constraints/constrainable_list.hpp"

using namespace rive;

ConstrainableList* ConstrainableList::from(Component* component)
{
    switch (component->coreType())
    {
        case ArtboardComponentListBase::typeKey:
            return component->as<ArtboardComponentList>();
    }
    return nullptr;
}

void ConstrainableList::addListConstraint(ListConstraint* constraint)
{
    assert(std::find(m_listConstraints.begin(),
                     m_listConstraints.end(),
                     constraint) == m_listConstraints.end());
    m_listConstraints.push_back(constraint);
}