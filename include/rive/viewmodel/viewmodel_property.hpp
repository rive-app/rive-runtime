#ifndef _RIVE_VIEW_MODEL_PROPERTY_HPP_
#define _RIVE_VIEW_MODEL_PROPERTY_HPP_
#include "rive/generated/viewmodel/viewmodel_property_base.hpp"
#include <stdio.h>
namespace rive
{
class ViewModelProperty : public ViewModelPropertyBase
{
public:
    enum class Direction : uint8_t
    {
        none = 0,
        input = 1,
        output = 2,
        both = 3,
    };

    StatusCode import(ImportStack& importStack) override;
    inline const std::string& constName() const { return m_Name; }

    /// Direction is stored in bits 0–1 of componentProps.
    Direction direction() const
    {
        return static_cast<Direction>(componentProps() & 0x3);
    }

    bool isInput() const
    {
        auto d = direction();
        return d == Direction::input || d == Direction::both;
    }

    bool isOutput() const
    {
        auto d = direction();
        return d == Direction::output || d == Direction::both;
    }
};
} // namespace rive

#endif