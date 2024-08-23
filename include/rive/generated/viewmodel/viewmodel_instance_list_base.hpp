#ifndef _RIVE_VIEW_MODEL_INSTANCE_LIST_BASE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_LIST_BASE_HPP_
#include "rive/viewmodel/viewmodel_instance_value.hpp"
namespace rive
{
class ViewModelInstanceListBase : public ViewModelInstanceValue
{
protected:
    typedef ViewModelInstanceValue Super;

public:
    static const uint16_t typeKey = 441;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelInstanceListBase::typeKey:
            case ViewModelInstanceValueBase::typeKey:
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