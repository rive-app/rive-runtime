#ifndef _RIVE_SOLO_HPP_
#define _RIVE_SOLO_HPP_
#include "rive/generated/solo_base.hpp"
namespace rive
{
class Solo : public SoloBase
{
public:
    void activeComponentIdChanged() override;
    StatusCode onAddedClean(CoreContext* context) override;
    bool collapse(bool value) override;
    void updateByIndex(size_t index);
    void updateByName(const std::string& name);
    int getActiveChildIndex();
    std::string getActiveChildName();
    // The child the Solo currently exposes; the layout descends into it.
    Component* activeComponent();

    // A Solo is transparent to layout — like a group, but it only
    // lets its *active* child through. It provides no layout node and no sizing
    // of its own; the layout above descends into the active child.
#ifdef WITH_RIVE_LAYOUT
    void recollectOwningLayout();
#endif

private:
    void propagateCollapse(bool collapse);
};
} // namespace rive

#endif
