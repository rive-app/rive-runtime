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

private:
    void propagateCollapse(bool collapse);
};
} // namespace rive

#endif