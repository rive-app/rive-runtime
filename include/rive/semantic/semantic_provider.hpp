#ifndef _RIVE_SEMANTIC_PROVIDER_HPP_
#define _RIVE_SEMANTIC_PROVIDER_HPP_

#include "rive/math/aabb.hpp"
#include <cstdint>
#include <string>

namespace rive
{
class Artboard;
class Component;

struct ResolvedSemanticData
{
    bool hasSemantics = false;
    uint32_t role = 0;
    std::string label = "";
};

class SemanticProvider
{
public:
    // Returns true if this component can have inferred semantics (e.g. Text).
    static bool canInferSemantics(Component* component);
    // Resolve semantic data from a SemanticData component child.
    static ResolvedSemanticData resolveSemanticData(Component* component);
    // Compute bounds in root artboard space.
    static AABB semanticBounds(Component* component);
    // Map an AABB in the given artboard's space through its rootTransform
    // to the outermost artboard's space. All four corners are mapped and
    // re-enclosed so rotation/shear in nested hosts is handled correctly.
    static AABB rootTransformAABB(Artboard* ab, const AABB& bounds);
};
} // namespace rive

#endif
