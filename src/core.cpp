#include "rive/core.hpp"

#include "rive/component_dirt.hpp"
#include "rive/data_bind/data_bind.hpp"

#include <cassert>

using namespace rive;

Core::~Core()
{
    // Detach any DataBind observers so they don't dangle. Walk the intrusive
    // list and null out each observer's target+chain pointer.
    DataBind* o = m_firstObserver;
    while (o != nullptr)
    {
        DataBind* next = o->nextObserver();
        o->onTargetDestroyed();
        o = next;
    }
    m_firstObserver = nullptr;
}

void Core::notifyPropertyChanged(uint16_t propertyKey)
{
    // Hot path: no subscribers — single branch.
    if (m_firstObserver == nullptr)
    {
        return;
    }
    for (DataBind* o = m_firstObserver; o != nullptr; o = o->nextObserver())
    {
        if (o->propertyKey() == propertyKey)
        {
            o->addDirt(ComponentDirt::Bindings, false);
        }
    }
}

void Core::addPropertyObserver(DataBind* observer)
{
    // Prepend to the list — O(1). Order doesn't matter; notifyPropertyChanged
    // walks all of them.
    //
    // Guard: if the caller subscribes the same observer twice without
    // unsubscribing in between, the list forms a cycle and
    // notifyPropertyChanged loops forever. Walk first to confirm absence;
    // observer counts per (target, key) are typically ≤1 so this is cheap.
    for (DataBind* o = m_firstObserver; o != nullptr; o = o->nextObserver())
    {
        if (o == observer)
        {
            assert(false &&
                   "DataBind already subscribed — double subscribe would "
                   "form a list cycle");
            return;
        }
    }
    observer->setNextObserver(m_firstObserver);
    m_firstObserver = observer;
}

void Core::removePropertyObserver(DataBind* observer)
{
    DataBind** link = &m_firstObserver;
    while (*link != nullptr)
    {
        if (*link == observer)
        {
            *link = observer->nextObserver();
            observer->setNextObserver(nullptr);
            return;
        }
        link = &(*link)->nextObserverRef();
    }
}
