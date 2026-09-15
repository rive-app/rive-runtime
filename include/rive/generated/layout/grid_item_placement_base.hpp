#ifndef _RIVE_GRID_ITEM_PLACEMENT_BASE_HPP_
#define _RIVE_GRID_ITEM_PLACEMENT_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_int_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class GridItemPlacementBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 1068;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case GridItemPlacementBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t gridColumnPropertyKey = 1047;
    static const uint16_t gridRowPropertyKey = 1048;
    static const uint16_t gridColumnSpanPropertyKey = 1049;
    static const uint16_t gridRowSpanPropertyKey = 1050;

protected:
    int16_t m_GridColumn = 0;
    int16_t m_GridRow = 0;
    uint16_t m_GridColumnSpan = 1;
    uint16_t m_GridRowSpan = 1;

public:
    inline int16_t gridColumn() const { return m_GridColumn; }
    void gridColumn(int16_t value)
    {
        if (m_GridColumn == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(gridColumnPropertyKey, &m_GridColumn, &value);
        m_GridColumn = value;
        RIVE_EDITOR_CHANGED(gridColumnChanged());
        notifyPropertyChanged(gridColumnPropertyKey);
    }

    inline int16_t gridRow() const { return m_GridRow; }
    void gridRow(int16_t value)
    {
        if (m_GridRow == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(gridRowPropertyKey, &m_GridRow, &value);
        m_GridRow = value;
        RIVE_EDITOR_CHANGED(gridRowChanged());
        notifyPropertyChanged(gridRowPropertyKey);
    }

    inline uint16_t gridColumnSpan() const { return m_GridColumnSpan; }
    void gridColumnSpan(uint16_t value)
    {
        if (m_GridColumnSpan == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(gridColumnSpanPropertyKey,
                             &m_GridColumnSpan,
                             &value);
        m_GridColumnSpan = value;
        RIVE_EDITOR_CHANGED(gridColumnSpanChanged());
        notifyPropertyChanged(gridColumnSpanPropertyKey);
    }

    inline uint16_t gridRowSpan() const { return m_GridRowSpan; }
    void gridRowSpan(uint16_t value)
    {
        if (m_GridRowSpan == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(gridRowSpanPropertyKey, &m_GridRowSpan, &value);
        m_GridRowSpan = value;
        RIVE_EDITOR_CHANGED(gridRowSpanChanged());
        notifyPropertyChanged(gridRowSpanPropertyKey);
    }

    Core* clone() const override;
    void copy(const GridItemPlacementBase& object)
    {
        m_GridColumn = object.m_GridColumn;
        m_GridRow = object.m_GridRow;
        m_GridColumnSpan = object.m_GridColumnSpan;
        m_GridRowSpan = object.m_GridRowSpan;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case gridColumnPropertyKey:
                m_GridColumn = CoreIntType::deserialize(reader);
                return true;
            case gridRowPropertyKey:
                m_GridRow = CoreIntType::deserialize(reader);
                return true;
            case gridColumnSpanPropertyKey:
                m_GridColumnSpan = CoreUintType::deserialize(reader);
                return true;
            case gridRowSpanPropertyKey:
                m_GridRowSpan = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void gridColumnChanged() {}
    virtual void gridRowChanged() {}
    virtual void gridColumnSpanChanged() {}
    virtual void gridRowSpanChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/layout/grid_item_placement_ext.inl"
#endif
};
} // namespace rive

#endif