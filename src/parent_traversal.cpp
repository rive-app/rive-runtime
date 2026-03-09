#include "rive/parent_traversal.hpp"
#include "rive/artboard.hpp"
#include "rive/artboard_host.hpp"
#include "rive/component.hpp"
#include "rive/container_component.hpp"

using namespace rive;

ParentTraversal::ParentTraversal(Component* start) :
    m_current(start != nullptr ? start->parent() : nullptr),
    m_currentArtboard(start != nullptr ? start->artboard() : nullptr)
{}

ContainerComponent* ParentTraversal::next()
{
    // Reset boundary crossing state
    m_didCrossBoundary = false;
    m_crossingHost = nullptr;
    m_sourceArtboard = nullptr;

    if (m_current == nullptr)
    {
        return nullptr;
    }

    ContainerComponent* result = m_current;

    // Advance to next parent
    if (m_current->parent() != nullptr)
    {
        m_current = m_current->parent();
    }
    else if (m_currentArtboard != nullptr &&
             m_currentArtboard->host() != nullptr)
    {
        // No direct parent - try crossing artboard boundary
        auto* host = m_currentArtboard->host();
        auto* hostComponent = host->hostComponent();

        if (hostComponent != nullptr && hostComponent->parent() != nullptr)
        {
            // Record boundary crossing info before updating state
            m_didCrossBoundary = true;
            m_crossingHost = host;
            m_sourceArtboard = m_currentArtboard;

            m_current = hostComponent->parent();
            m_currentArtboard = host->parentArtboard();
        }
        else
        {
            m_current = nullptr;
        }
    }
    else
    {
        m_current = nullptr;
    }

    return result;
}
