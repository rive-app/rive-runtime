#ifndef _RIVE_FOCUS_INPUT_TRAVERSAL_HPP_
#define _RIVE_FOCUS_INPUT_TRAVERSAL_HPP_

#include "rive/component.hpp"
#include "rive/focus_data.hpp"
#include "rive/input/focusable.hpp"
#include "rive/node.hpp"

namespace rive
{

/// Recursively send input to Focusable children in hierarchy order.
/// Uses a member function pointer template for zero-cost abstraction.
///
/// The traversal:
/// - Stops and returns true if a Focusable handles the input
/// - Does not traverse into Focusable subtrees (they handle their own
///   delegation)
/// - Skips subtrees that have FocusData (they receive callbacks separately)
///
/// @param component The component to start traversal from
/// @param method Member function pointer to call on Focusable (e.g.,
///               &Focusable::keyInput)
/// @param args Arguments to pass to the method
/// @return true if input was handled, false otherwise
template <typename Method, typename... Args>
bool sendInputToFocusableChildren(Component* component,
                                  Method method,
                                  Args... args)
{
    if (component == nullptr)
    {
        return false;
    }

    // Check if this component implements Focusable
    auto* focusable = Focusable::from(component);
    if (focusable != nullptr)
    {
        if ((focusable->*method)(args...))
        {
            return true;
        }
        // Focusable handles its own internal delegation, don't traverse into it
        return false;
    }

    // Recursively check children if this is a Node
    if (component->is<Node>())
    {
        auto node = component->as<Node>();
        // If the node has focus data then it will have already traversed this
        // sub-tree in a previous callback.
        if (node->firstChild<FocusData>() == nullptr)
        {
            for (auto* child : node->children())
            {
                if (sendInputToFocusableChildren(child, method, args...))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

} // namespace rive

#endif
