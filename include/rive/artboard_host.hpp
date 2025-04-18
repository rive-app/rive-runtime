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

class ArtboardHost
{
public:
    virtual size_t artboardCount() = 0;
    virtual ArtboardInstance* artboardInstance(int index = 0) = 0;
    virtual std::vector<uint32_t> dataBindPathIds() { return {}; }
    virtual void populateDataBinds(std::vector<DataBind*>* dataBinds) = 0;
    virtual void internalDataContext(DataContext* dataContext) = 0;
    virtual void bindViewModelInstance(rcp<ViewModelInstance> viewModelInstance,
                                       DataContext* parent) = 0;
    virtual void clearDataContext() = 0;
    virtual void markHostingLayoutDirty(ArtboardInstance* artboardInstance) {}
    // The artboard that contains this ArtboardHost
    virtual Artboard* parentArtboard() = 0;
    virtual void markHostTransformDirty() = 0;
    virtual bool isLayoutProvider() { return false; }
};
} // namespace rive

#endif