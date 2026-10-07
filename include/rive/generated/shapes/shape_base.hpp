#ifndef _RIVE_SHAPE_BASE_HPP_
#define _RIVE_SHAPE_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/drawable.hpp"
namespace rive
{
class ShapeBase : public Drawable
{
protected:
    typedef Drawable Super;

public:
    static const uint16_t typeKey = 3;

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t lengthPropertyKey = 781;

protected:
public:
    virtual void setLength(float value) = 0;
    virtual float length() = 0;
    void length(float value)
    {
        if (length() == value)
        {
            return;
        }
        setLength(value);
        lengthChanged();
        notifyPropertyChanged(lengthPropertyKey);
    }

    Core* clone() const override;

protected:
    virtual void lengthChanged() {}
};
} // namespace rive

#endif