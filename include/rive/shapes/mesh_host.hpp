#ifndef _RIVE_MESH_HOST_HPP_
#define _RIVE_MESH_HOST_HPP_
#include "rive/math/mat2d.hpp"
#include "rive/math/vec2d.hpp"

namespace rive
{
class Component;
class MeshDrawable;
class RenderImage;

// What a Mesh or NSlicer needs from an Image or Node-mode NestedArtboard.
class MeshHost
{
public:
    virtual ~MeshHost() = default;

    // Exact core types: Leaf and Layout subclass NestedArtboard but can't host.
    static MeshHost* from(Component* component);

    virtual void setMesh(MeshDrawable* mesh) = 0;
    virtual MeshDrawable* hostedMesh() const = 0;

    virtual float meshHostWidth() const = 0;
    virtual float meshHostHeight() const = 0;
    // Where local (0,0) sits in the image, as a fraction of its size.
    virtual Vec2D meshHostOrigin() const = 0;
    virtual float meshHostScaleX() const = 0;
    virtual float meshHostScaleY() const = 0;
    // Local to world for an unskinned mesh; skinned vertices ignore it.
    virtual Mat2D meshHostWorldTransform() const = 0;
    virtual bool meshHostReady() const = 0;
    // Maps 0..1 UVs onto the sampled texture; identity until it exists.
    virtual Mat2D meshHostUVTransform() const = 0;

#ifdef WITH_RIVE_EDITOR
    virtual void setMeshForEditor(MeshDrawable* mesh) = 0;
    // No-op unless the mesh is still `expected` (a newer sibling may own it).
    virtual void clearMeshIfForEditor(MeshDrawable* expected) = 0;
#endif
};
} // namespace rive

#endif
