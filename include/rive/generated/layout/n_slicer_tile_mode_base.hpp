#ifndef _RIVE_N_SLICER_TILE_MODE_BASE_HPP_
#define _RIVE_N_SLICER_TILE_MODE_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class NSlicerTileModeBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 491;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case NSlicerTileModeBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t patchIndexPropertyKey = 672;
    static const uint16_t stylePropertyKey = 673;

private:
    uint32_t m_PatchIndex = 0;
    uint32_t m_Style = 0;

public:
    inline uint32_t patchIndex() const { return m_PatchIndex; }
    void patchIndex(uint32_t value)
    {
        if (m_PatchIndex == value)
        {
            return;
        }
        m_PatchIndex = value;
        patchIndexChanged();
    }

    inline uint32_t style() const { return m_Style; }
    void style(uint32_t value)
    {
        if (m_Style == value)
        {
            return;
        }
        m_Style = value;
        styleChanged();
    }

    Core* clone() const override;
    void copy(const NSlicerTileModeBase& object)
    {
        m_PatchIndex = object.m_PatchIndex;
        m_Style = object.m_Style;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case patchIndexPropertyKey:
                m_PatchIndex = CoreUintType::deserialize(reader);
                return true;
            case stylePropertyKey:
                m_Style = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void patchIndexChanged() {}
    virtual void styleChanged() {}
};
} // namespace rive

#endif