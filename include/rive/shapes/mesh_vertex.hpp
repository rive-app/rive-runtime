#ifndef _RIVE_MESH_VERTEX_HPP_
#define _RIVE_MESH_VERTEX_HPP_
#include "rive/generated/shapes/mesh_vertex_base.hpp"
#include <stdio.h>
namespace rive
{
class MeshVertex : public MeshVertexBase
{
public:
    void markGeometryDirty() override;
    StatusCode onAddedDirty(CoreContext* context) override;
#ifdef WITH_RIVE_EDITOR
    // See `Component::editorParentChanged` for the lifecycle
    // contract. Body in `editor_native/.../mesh_vertex_editor.cpp`.
    void editorParentChanged(ContainerComponent* from,
                             ContainerComponent* to) override;
#endif
};
} // namespace rive

#endif