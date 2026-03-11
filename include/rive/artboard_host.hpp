#ifndef _RIVE_ARTBOARD_HOST_HPP_
#define _RIVE_ARTBOARD_HOST_HPP_
#include "rive/refcnt.hpp"
#include "rive/file.hpp"
#include "rive/data_bind_path_referencer.hpp"
#include "rive/math/mat2d.hpp"
#include <stdio.h>
namespace rive
{
class ArtboardInstance;
class DataBind;
class DataContext;
class ViewModelInstance;

class ArtboardHost : public DataBindPathReferencer
{
public:
    virtual size_t artboardCount() = 0;
    virtual ArtboardInstance* artboardInstance(int index = 0) = 0;
    virtual void internalDataContext(rcp<DataContext> dataContext) = 0;
    virtual void bindViewModelInstance(rcp<ViewModelInstance> viewModelInstance,
                                       rcp<DataContext> parent) = 0;
    virtual void clearDataContext() = 0;
    virtual void unbind() = 0;
    virtual void updateDataBinds() = 0;
    virtual void markHostingLayoutDirty(ArtboardInstance* artboardInstance) {}
    // The artboard that contains this ArtboardHost
    virtual Artboard* parentArtboard() = 0;
    virtual bool hitTestHost(const Vec2D& position,
                             bool skipOnUnclipped,
                             ArtboardInstance* artboard) = 0;
    virtual Vec2D hostTransformPoint(const Vec2D&, ArtboardInstance*) = 0;
    /// Returns the transform matrix from nested artboard space to parent
    /// artboard space. Unlike hostTransformPoint, this does NOT include
    /// rootTransform.
    virtual Mat2D worldTransformForArtboard(ArtboardInstance*) = 0;
    virtual void markHostTransformDirty() = 0;
    virtual bool isLayoutProvider() { return false; }
    virtual void file(File* value) = 0;
    virtual File* file() const = 0;

    /// Return this host as a Component, if applicable (e.g., NestedArtboard).
    /// Returns nullptr if the host is not a Component.
    virtual class Component* hostComponent() { return nullptr; }
    virtual void relinkDataContext(rcp<ViewModelInstance> viewModelInstance) {}
};
} // namespace rive

#endif