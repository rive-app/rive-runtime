#ifndef _RIVE_PARENT_TRAVERSAL_HPP_
#define _RIVE_PARENT_TRAVERSAL_HPP_

namespace rive
{
class Artboard;
class ArtboardHost;
class ArtboardInstance;
class Component;
class ContainerComponent;

/// Helper class for traversing up the parent hierarchy, automatically
/// crossing artboard boundaries when needed (e.g., from a nested artboard
/// to its parent artboard via ArtboardHost).
///
/// Usage:
///   ParentTraversal traversal(startComponent);
///   while (auto* parent = traversal.next())
///   {
///       // Process parent...
///   }
///
/// For traversals that need to know when artboard boundaries are crossed
/// (e.g., to transform coordinates), check didCrossBoundary() after each
/// next() call. When true, crossingHost() and sourceArtboard() provide
/// the information needed to transform between artboard spaces.
class ParentTraversal
{
public:
    /// Start traversal from the given component.
    /// The first call to next() will return the component's parent.
    /// The component's artboard is used to detect when we need to
    /// cross artboard boundaries.
    ParentTraversal(Component* start);

    /// Move to the next parent in the hierarchy, crossing artboard
    /// boundaries when needed. Returns the next parent, or nullptr
    /// when the traversal is complete.
    ContainerComponent* next();

    /// Returns the artboard context for the current position in the traversal.
    /// This changes when crossing artboard boundaries.
    Artboard* currentArtboard() const { return m_currentArtboard; }

    /// Returns true if the last call to next() crossed an artboard boundary.
    bool didCrossBoundary() const { return m_didCrossBoundary; }

    /// When didCrossBoundary() is true, returns the ArtboardHost that was
    /// used to cross the boundary. Use this with sourceArtboard() to get
    /// the world transform for coordinate conversions.
    ArtboardHost* crossingHost() const { return m_crossingHost; }

    /// When didCrossBoundary() is true, returns the artboard we crossed FROM.
    /// Cast to ArtboardInstance if needed for worldTransformForArtboard().
    Artboard* sourceArtboard() const { return m_sourceArtboard; }

private:
    ContainerComponent* m_current;
    Artboard* m_currentArtboard;

    // Boundary crossing state (valid after next() that crosses a boundary)
    bool m_didCrossBoundary = false;
    ArtboardHost* m_crossingHost = nullptr;
    Artboard* m_sourceArtboard = nullptr;
};

} // namespace rive

#endif
