#ifndef _RIVE_ARTBOARD_HOST_HPP_
#define _RIVE_ARTBOARD_HOST_HPP_
#include "rive/refcnt.hpp"
#include <stdio.h>
namespace rive
{
class ArtboardInstance;
class DataBind;
class DataContext;
class ViewModelInstance;
class File;

class ArtboardHost
{
public:
    virtual size_t artboardCount() = 0;
    virtual ArtboardInstance* artboardInstance(int index = 0) = 0;
    virtual std::vector<uint32_t> dataBindPathIds() { return {}; }
    virtual void internalDataContext(DataContext* dataContext) = 0;
    virtual void bindViewModelInstance(rcp<ViewModelInstance> viewModelInstance,
                                       DataContext* parent) = 0;
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
    virtual void markHostTransformDirty() = 0;
    virtual bool isLayoutProvider() { return false; }
    virtual void file(File* value) = 0;
    virtual File* file() const = 0;
};
} // namespace rive

#endif