#ifndef _RIVE_CUBIC_VALUE_INTERPOLATOR_BASE_HPP_
#define _RIVE_CUBIC_VALUE_INTERPOLATOR_BASE_HPP_
#include "rive/animation/cubic_interpolator.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class CubicValueInterpolatorBase : public CubicInterpolator
{
protected:
    typedef CubicInterpolator Super;

public:
    static const uint16_t typeKey = 138;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case CubicValueInterpolatorBase::typeKey:
            case CubicInterpolatorBase::typeKey:
            case KeyFrameInterpolatorBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

public:
    Core* clone() const override;
    void copy(const CubicValueInterpolatorBase& object)
    {
        RIVE_EDITOR_COPY(object);
        CubicInterpolator::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return CubicInterpolator::deserialize(propertyKey, reader);
    }
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/cubic_value_interpolator_ext.inl"
#endif
};
} // namespace rive

#endif