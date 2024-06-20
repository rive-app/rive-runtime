#ifndef _RIVE_VIEW_MODEL_PROPERTY_BASE_HPP_
#define _RIVE_VIEW_MODEL_PROPERTY_BASE_HPP_
#include "rive/viewmodel/viewmodel_component.hpp"
namespace rive
{
class ViewModelPropertyBase : public ViewModelComponent
{
protected:
    typedef ViewModelComponent Super;

public:
    static const uint16_t typeKey = 430;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelPropertyBase::typeKey:
            case ViewModelComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;

protected:
};
} // namespace rive

#endif