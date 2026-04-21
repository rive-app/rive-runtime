#include "rive/semantic/semantic_provider.hpp"

#include "rive/artboard.hpp"
#include "rive/component.hpp"
#include "rive/container_component.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/node.hpp"
#include "rive/semantic/semantic_data.hpp"
#include "semantic_inference_registry.hpp"

using namespace rive;

static bool tryNodeWorldBounds(Node* node, AABB& outBounds)
{
    if (node == nullptr)
    {
        return false;
    }

    const auto localBounds = node->localBounds();
    if (localBounds.isEmptyOrNaN())
    {
        return false;
    }
    const auto worldTransform = node->worldTransform();
    const auto worldBounds = worldTransform.mapBoundingBox(localBounds);
    if (worldBounds.isEmptyOrNaN())
    {
        return false;
    }
    outBounds = worldBounds;
    return true;
}

// Map a world-space AABB through the artboard's rootTransform into root
// (parent-artboard) space. rootTransform can include rotation/shear (nested
// artboards with rotated hosts), so all four corners must be mapped and
// re-enclosed — mapping only min/max leaves the other two corners outside
// the resulting rect under rotation.
static AABB rootTransformAABB(Artboard* ab, const AABB& bounds)
{
    AABB out = AABB::forExpansion();
    AABB::expandTo(out, ab->rootTransform({bounds.minX, bounds.minY}));
    AABB::expandTo(out, ab->rootTransform({bounds.maxX, bounds.minY}));
    AABB::expandTo(out, ab->rootTransform({bounds.maxX, bounds.maxY}));
    AABB::expandTo(out, ab->rootTransform({bounds.minX, bounds.maxY}));
    return out;
}

bool SemanticProvider::canInferSemantics(Component* component)
{
    return supportsInferredSemantics(component);
}

ResolvedSemanticData SemanticProvider::resolveSemanticData(Component* component)
{
    ResolvedSemanticData out;
    if (component == nullptr || !component->is<Node>())
    {
        return out;
    }

    if (auto* sd = component->as<Node>()->firstChild<SemanticData>())
    {
        out.hasSemantics = true;
        out.role = sd->role();
        out.label = sd->label();
        return out;
    }

    // Inference fallback (Text -> label)
    resolveInferredSemantics(component, out);

    return out;
}

// Computes world-space bounds for a component. Three strategies:
// 1. Drawable nodes: localBounds() → worldTransform → rootTransform
// 2. Container nodes: merge bounds of all direct Node children
// 3. Fallback: world transform translation (degenerate point bounds)
AABB SemanticProvider::semanticBounds(Component* component)
{
    if (component == nullptr || !component->is<Node>())
    {
        return AABB();
    }

    auto* node = component->as<Node>();

    AABB worldBounds;
    if (tryNodeWorldBounds(node, worldBounds))
    {
        auto* ab = node->artboard();
        if (ab != nullptr)
        {
            return rootTransformAABB(ab, worldBounds);
        }
        return worldBounds;
    }

    // If this semantic node is a non-drawable container/group, derive bounds
    // from visual descendant nodes. ContainerComponent::forEachChild walks
    // the whole subtree (not just direct children): the predicate runs for
    // each descendant, and returning `true` tells forEachChild to keep
    // descending into that descendant's children. We always return true so
    // every Node descendant contributes bounds; non-Node descendants and
    // Nodes without valid bounds are skipped but their subtrees are still
    // walked.
    if (component->is<ContainerComponent>())
    {
        AABB merged = AABB::forExpansion();
        bool hasDescendantBounds = false;
        component->as<ContainerComponent>()->forEachChild(
            [&](Component* child) -> bool {
                if (child == nullptr || !child->is<Node>())
                {
                    return true;
                }

                AABB childBounds;
                if (!tryNodeWorldBounds(child->as<Node>(), childBounds))
                {
                    return true;
                }

                merged.expand(childBounds);
                hasDescendantBounds = true;
                return true;
            });

        if (hasDescendantBounds)
        {
            auto* ab = node->artboard();
            if (ab != nullptr)
            {
                return rootTransformAABB(ab, merged);
            }
            return merged;
        }
    }

    // Fallback for nodes/components without local bounds.
    const auto worldTransform = node->worldTransform();
    const float x = worldTransform[4];
    const float y = worldTransform[5];
    auto* ab = node->artboard();
    if (ab != nullptr)
    {
        Vec2D pos = ab->rootTransform(Vec2D(x, y));
        return AABB(pos.x, pos.y, pos.x, pos.y);
    }
    return AABB(x, y, x, y);
}
