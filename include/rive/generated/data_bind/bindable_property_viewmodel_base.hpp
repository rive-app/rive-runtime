#ifndef _RIVE_BINDABLE_PROPERTY_VIEW_MODEL_BASE_HPP_
#define _RIVE_BINDABLE_PROPERTY_VIEW_MODEL_BASE_HPP_
#include "rive/data_bind/bindable_property_id.hpp"
namespace rive
{
class BindablePropertyViewModelBase : public BindablePropertyId
{
protected:
    typedef BindablePropertyId Super;

public:
    static const uint16_t typeKey = 662;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif