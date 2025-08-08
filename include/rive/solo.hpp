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
    void updateByName(std::string& name);
    int getActiveChildIndex();
    std::string getActiveChildName();

private:
    void propagateCollapse(bool collapse);
};
} // namespace rive

#endif