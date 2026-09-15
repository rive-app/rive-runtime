#ifndef _RIVE_SEMANTIC_DATA_BASE_HPP_
#define _RIVE_SEMANTIC_DATA_BASE_HPP_
#include <string>
#include "rive/component.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class SemanticDataBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 668;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case SemanticDataBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t rolePropertyKey = 982;
    static const uint16_t labelPropertyKey = 983;
    static const uint16_t valuePropertyKey = 984;
    static const uint16_t hintPropertyKey = 985;
    static const uint16_t headingLevelPropertyKey = 986;
    static const uint16_t traitFlagsPropertyKey = 987;
    static const uint16_t stateFlagsPropertyKey = 988;
    static const uint16_t isExpandablePropertyKey = 989;
    static const uint32_t isExpandableBitmask = 1u << 0;
    static const uint16_t isSelectablePropertyKey = 990;
    static const uint32_t isSelectableBitmask = 1u << 1;
    static const uint16_t isCheckablePropertyKey = 991;
    static const uint32_t isCheckableBitmask = 1u << 2;
    static const uint16_t isToggleablePropertyKey = 992;
    static const uint32_t isToggleableBitmask = 1u << 3;
    static const uint16_t isRequirablePropertyKey = 993;
    static const uint32_t isRequirableBitmask = 1u << 4;
    static const uint16_t isEnablablePropertyKey = 994;
    static const uint32_t isEnablableBitmask = 1u << 5;
    static const uint16_t isFocusablePropertyKey = 995;
    static const uint32_t isFocusableBitmask = 1u << 6;
    static const uint16_t isExpandedPropertyKey = 996;
    static const uint32_t isExpandedBitmask = 1u << 0;
    static const uint16_t isSelectedPropertyKey = 997;
    static const uint32_t isSelectedBitmask = 1u << 1;
    static const uint16_t isCheckedPropertyKey = 998;
    static const uint32_t isCheckedBitOffset = 2;
    static const uint32_t isCheckedFieldMask = 12u;
    static const uint16_t isToggledPropertyKey = 1000;
    static const uint32_t isToggledBitmask = 1u << 4;
    static const uint16_t isRequiredPropertyKey = 1001;
    static const uint32_t isRequiredBitmask = 1u << 5;
    static const uint16_t isDisabledPropertyKey = 1002;
    static const uint32_t isDisabledBitmask = 1u << 6;
    static const uint16_t isFocusedPropertyKey = 1003;
    static const uint32_t isFocusedBitmask = 1u << 7;
    static const uint16_t isHiddenPropertyKey = 1004;
    static const uint32_t isHiddenBitmask = 1u << 8;
    static const uint16_t isLiveRegionPropertyKey = 1005;
    static const uint32_t isLiveRegionBitmask = 1u << 9;
    static const uint16_t isReadOnlyPropertyKey = 1006;
    static const uint32_t isReadOnlyBitmask = 1u << 10;
    static const uint16_t isModalPropertyKey = 1007;
    static const uint32_t isModalBitmask = 1u << 11;
    static const uint16_t isObscuredPropertyKey = 1008;
    static const uint32_t isObscuredBitmask = 1u << 12;
    static const uint16_t isMultilinePropertyKey = 1009;
    static const uint32_t isMultilineBitmask = 1u << 13;

protected:
    uint32_t m_Role = 0;
    std::string m_Label = "";
    std::string m_Value = "";
    std::string m_Hint = "";
    uint32_t m_HeadingLevel = 0;
    uint32_t m_TraitFlags = 0;
    uint32_t m_StateFlags = 0;

public:
    inline uint32_t role() const { return m_Role; }
    void role(uint32_t value)
    {
        if (m_Role == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(rolePropertyKey, &m_Role, &value);
        m_Role = value;
        RIVE_EDITOR_CHANGED(roleChanged());
        notifyPropertyChanged(rolePropertyKey);
    }

    inline const std::string& label() const { return m_Label; }
    void label(std::string value)
    {
        if (m_Label == value)
        {
            return;
        }
        RIVE_EDITOR_STRING_CHANGING(labelPropertyKey, m_Label, value);
        m_Label = value;
        RIVE_EDITOR_CHANGED(labelChanged());
        notifyPropertyChanged(labelPropertyKey);
    }

    inline const std::string& value() const { return m_Value; }
    void value(std::string value)
    {
        if (m_Value == value)
        {
            return;
        }
        RIVE_EDITOR_STRING_CHANGING(valuePropertyKey, m_Value, value);
        m_Value = value;
        RIVE_EDITOR_CHANGED(valueChanged());
        notifyPropertyChanged(valuePropertyKey);
    }

    inline const std::string& hint() const { return m_Hint; }
    void hint(std::string value)
    {
        if (m_Hint == value)
        {
            return;
        }
        RIVE_EDITOR_STRING_CHANGING(hintPropertyKey, m_Hint, value);
        m_Hint = value;
        RIVE_EDITOR_CHANGED(hintChanged());
        notifyPropertyChanged(hintPropertyKey);
    }

    inline uint32_t headingLevel() const { return m_HeadingLevel; }
    void headingLevel(uint32_t value)
    {
        if (m_HeadingLevel == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(headingLevelPropertyKey, &m_HeadingLevel, &value);
        m_HeadingLevel = value;
        RIVE_EDITOR_CHANGED(headingLevelChanged());
        notifyPropertyChanged(headingLevelPropertyKey);
    }

    inline uint32_t traitFlags() const { return m_TraitFlags; }
    void traitFlags(uint32_t value)
    {
        if (m_TraitFlags == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(traitFlagsPropertyKey, &m_TraitFlags, &value);
        m_TraitFlags = value;
        RIVE_EDITOR_CHANGED(traitFlagsChanged());
        notifyPropertyChanged(traitFlagsPropertyKey);
    }

    inline uint32_t stateFlags() const { return m_StateFlags; }
    void stateFlags(uint32_t value)
    {
        if (m_StateFlags == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(stateFlagsPropertyKey, &m_StateFlags, &value);
        m_StateFlags = value;
        RIVE_EDITOR_CHANGED(stateFlagsChanged());
        notifyPropertyChanged(stateFlagsPropertyKey);
    }

    inline bool isExpandable() const
    {
        return (m_TraitFlags & isExpandableBitmask) != 0;
    }
    void isExpandable(bool value)
    {
        const bool prev = (m_TraitFlags & isExpandableBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isExpandablePropertyKey, &prev, &value);
        m_TraitFlags = value ? (m_TraitFlags | isExpandableBitmask)
                             : (m_TraitFlags & ~isExpandableBitmask);
        RIVE_EDITOR_CHANGED(traitFlagsChanged());
        notifyPropertyChanged(traitFlagsPropertyKey);
    }
    inline bool isSelectable() const
    {
        return (m_TraitFlags & isSelectableBitmask) != 0;
    }
    void isSelectable(bool value)
    {
        const bool prev = (m_TraitFlags & isSelectableBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isSelectablePropertyKey, &prev, &value);
        m_TraitFlags = value ? (m_TraitFlags | isSelectableBitmask)
                             : (m_TraitFlags & ~isSelectableBitmask);
        RIVE_EDITOR_CHANGED(traitFlagsChanged());
        notifyPropertyChanged(traitFlagsPropertyKey);
    }
    inline bool isCheckable() const
    {
        return (m_TraitFlags & isCheckableBitmask) != 0;
    }
    void isCheckable(bool value)
    {
        const bool prev = (m_TraitFlags & isCheckableBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isCheckablePropertyKey, &prev, &value);
        m_TraitFlags = value ? (m_TraitFlags | isCheckableBitmask)
                             : (m_TraitFlags & ~isCheckableBitmask);
        RIVE_EDITOR_CHANGED(traitFlagsChanged());
        notifyPropertyChanged(traitFlagsPropertyKey);
    }
    inline bool isToggleable() const
    {
        return (m_TraitFlags & isToggleableBitmask) != 0;
    }
    void isToggleable(bool value)
    {
        const bool prev = (m_TraitFlags & isToggleableBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isToggleablePropertyKey, &prev, &value);
        m_TraitFlags = value ? (m_TraitFlags | isToggleableBitmask)
                             : (m_TraitFlags & ~isToggleableBitmask);
        RIVE_EDITOR_CHANGED(traitFlagsChanged());
        notifyPropertyChanged(traitFlagsPropertyKey);
    }
    inline bool isRequirable() const
    {
        return (m_TraitFlags & isRequirableBitmask) != 0;
    }
    void isRequirable(bool value)
    {
        const bool prev = (m_TraitFlags & isRequirableBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isRequirablePropertyKey, &prev, &value);
        m_TraitFlags = value ? (m_TraitFlags | isRequirableBitmask)
                             : (m_TraitFlags & ~isRequirableBitmask);
        RIVE_EDITOR_CHANGED(traitFlagsChanged());
        notifyPropertyChanged(traitFlagsPropertyKey);
    }
    inline bool isEnablable() const
    {
        return (m_TraitFlags & isEnablableBitmask) != 0;
    }
    void isEnablable(bool value)
    {
        const bool prev = (m_TraitFlags & isEnablableBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isEnablablePropertyKey, &prev, &value);
        m_TraitFlags = value ? (m_TraitFlags | isEnablableBitmask)
                             : (m_TraitFlags & ~isEnablableBitmask);
        RIVE_EDITOR_CHANGED(traitFlagsChanged());
        notifyPropertyChanged(traitFlagsPropertyKey);
    }
    inline bool isFocusable() const
    {
        return (m_TraitFlags & isFocusableBitmask) != 0;
    }
    void isFocusable(bool value)
    {
        const bool prev = (m_TraitFlags & isFocusableBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isFocusablePropertyKey, &prev, &value);
        m_TraitFlags = value ? (m_TraitFlags | isFocusableBitmask)
                             : (m_TraitFlags & ~isFocusableBitmask);
        RIVE_EDITOR_CHANGED(traitFlagsChanged());
        notifyPropertyChanged(traitFlagsPropertyKey);
    }
    inline bool isExpanded() const
    {
        return (m_StateFlags & isExpandedBitmask) != 0;
    }
    void isExpanded(bool value)
    {
        const bool prev = (m_StateFlags & isExpandedBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isExpandedPropertyKey, &prev, &value);
        m_StateFlags = value ? (m_StateFlags | isExpandedBitmask)
                             : (m_StateFlags & ~isExpandedBitmask);
        RIVE_EDITOR_CHANGED(stateFlagsChanged());
        notifyPropertyChanged(stateFlagsPropertyKey);
    }
    inline bool isSelected() const
    {
        return (m_StateFlags & isSelectedBitmask) != 0;
    }
    void isSelected(bool value)
    {
        const bool prev = (m_StateFlags & isSelectedBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isSelectedPropertyKey, &prev, &value);
        m_StateFlags = value ? (m_StateFlags | isSelectedBitmask)
                             : (m_StateFlags & ~isSelectedBitmask);
        RIVE_EDITOR_CHANGED(stateFlagsChanged());
        notifyPropertyChanged(stateFlagsPropertyKey);
    }
    inline uint8_t isChecked() const
    {
        return (m_StateFlags & isCheckedFieldMask) >> isCheckedBitOffset;
    }
    void isChecked(uint8_t value)
    {
        const uint8_t prev =
            (m_StateFlags & isCheckedFieldMask) >> isCheckedBitOffset;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isCheckedPropertyKey, &prev, &value);
        m_StateFlags = (m_StateFlags & ~isCheckedFieldMask) |
                       ((value << isCheckedBitOffset) & isCheckedFieldMask);
        RIVE_EDITOR_CHANGED(stateFlagsChanged());
        notifyPropertyChanged(stateFlagsPropertyKey);
    }
    inline bool isToggled() const
    {
        return (m_StateFlags & isToggledBitmask) != 0;
    }
    void isToggled(bool value)
    {
        const bool prev = (m_StateFlags & isToggledBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isToggledPropertyKey, &prev, &value);
        m_StateFlags = value ? (m_StateFlags | isToggledBitmask)
                             : (m_StateFlags & ~isToggledBitmask);
        RIVE_EDITOR_CHANGED(stateFlagsChanged());
        notifyPropertyChanged(stateFlagsPropertyKey);
    }
    inline bool isRequired() const
    {
        return (m_StateFlags & isRequiredBitmask) != 0;
    }
    void isRequired(bool value)
    {
        const bool prev = (m_StateFlags & isRequiredBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isRequiredPropertyKey, &prev, &value);
        m_StateFlags = value ? (m_StateFlags | isRequiredBitmask)
                             : (m_StateFlags & ~isRequiredBitmask);
        RIVE_EDITOR_CHANGED(stateFlagsChanged());
        notifyPropertyChanged(stateFlagsPropertyKey);
    }
    inline bool isDisabled() const
    {
        return (m_StateFlags & isDisabledBitmask) != 0;
    }
    void isDisabled(bool value)
    {
        const bool prev = (m_StateFlags & isDisabledBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isDisabledPropertyKey, &prev, &value);
        m_StateFlags = value ? (m_StateFlags | isDisabledBitmask)
                             : (m_StateFlags & ~isDisabledBitmask);
        RIVE_EDITOR_CHANGED(stateFlagsChanged());
        notifyPropertyChanged(stateFlagsPropertyKey);
    }
    inline bool isFocused() const
    {
        return (m_StateFlags & isFocusedBitmask) != 0;
    }
    void isFocused(bool value)
    {
        const bool prev = (m_StateFlags & isFocusedBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isFocusedPropertyKey, &prev, &value);
        m_StateFlags = value ? (m_StateFlags | isFocusedBitmask)
                             : (m_StateFlags & ~isFocusedBitmask);
        RIVE_EDITOR_CHANGED(stateFlagsChanged());
        notifyPropertyChanged(stateFlagsPropertyKey);
    }
    inline bool isHidden() const
    {
        return (m_StateFlags & isHiddenBitmask) != 0;
    }
    void isHidden(bool value)
    {
        const bool prev = (m_StateFlags & isHiddenBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isHiddenPropertyKey, &prev, &value);
        m_StateFlags = value ? (m_StateFlags | isHiddenBitmask)
                             : (m_StateFlags & ~isHiddenBitmask);
        RIVE_EDITOR_CHANGED(stateFlagsChanged());
        notifyPropertyChanged(stateFlagsPropertyKey);
    }
    inline bool isLiveRegion() const
    {
        return (m_StateFlags & isLiveRegionBitmask) != 0;
    }
    void isLiveRegion(bool value)
    {
        const bool prev = (m_StateFlags & isLiveRegionBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isLiveRegionPropertyKey, &prev, &value);
        m_StateFlags = value ? (m_StateFlags | isLiveRegionBitmask)
                             : (m_StateFlags & ~isLiveRegionBitmask);
        RIVE_EDITOR_CHANGED(stateFlagsChanged());
        notifyPropertyChanged(stateFlagsPropertyKey);
    }
    inline bool isReadOnly() const
    {
        return (m_StateFlags & isReadOnlyBitmask) != 0;
    }
    void isReadOnly(bool value)
    {
        const bool prev = (m_StateFlags & isReadOnlyBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isReadOnlyPropertyKey, &prev, &value);
        m_StateFlags = value ? (m_StateFlags | isReadOnlyBitmask)
                             : (m_StateFlags & ~isReadOnlyBitmask);
        RIVE_EDITOR_CHANGED(stateFlagsChanged());
        notifyPropertyChanged(stateFlagsPropertyKey);
    }
    inline bool isModal() const { return (m_StateFlags & isModalBitmask) != 0; }
    void isModal(bool value)
    {
        const bool prev = (m_StateFlags & isModalBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isModalPropertyKey, &prev, &value);
        m_StateFlags = value ? (m_StateFlags | isModalBitmask)
                             : (m_StateFlags & ~isModalBitmask);
        RIVE_EDITOR_CHANGED(stateFlagsChanged());
        notifyPropertyChanged(stateFlagsPropertyKey);
    }
    inline bool isObscured() const
    {
        return (m_StateFlags & isObscuredBitmask) != 0;
    }
    void isObscured(bool value)
    {
        const bool prev = (m_StateFlags & isObscuredBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isObscuredPropertyKey, &prev, &value);
        m_StateFlags = value ? (m_StateFlags | isObscuredBitmask)
                             : (m_StateFlags & ~isObscuredBitmask);
        RIVE_EDITOR_CHANGED(stateFlagsChanged());
        notifyPropertyChanged(stateFlagsPropertyKey);
    }
    inline bool isMultiline() const
    {
        return (m_StateFlags & isMultilineBitmask) != 0;
    }
    void isMultiline(bool value)
    {
        const bool prev = (m_StateFlags & isMultilineBitmask) != 0;
        if (prev == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isMultilinePropertyKey, &prev, &value);
        m_StateFlags = value ? (m_StateFlags | isMultilineBitmask)
                             : (m_StateFlags & ~isMultilineBitmask);
        RIVE_EDITOR_CHANGED(stateFlagsChanged());
        notifyPropertyChanged(stateFlagsPropertyKey);
    }
    Core* clone() const override;
    void copy(const SemanticDataBase& object)
    {
        m_Role = object.m_Role;
        m_Label = object.m_Label;
        m_Value = object.m_Value;
        m_Hint = object.m_Hint;
        m_HeadingLevel = object.m_HeadingLevel;
        m_TraitFlags = object.m_TraitFlags;
        m_StateFlags = object.m_StateFlags;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case rolePropertyKey:
                m_Role = CoreUintType::deserialize(reader);
                return true;
            case labelPropertyKey:
                m_Label = CoreStringType::deserialize(reader);
                return true;
            case valuePropertyKey:
                m_Value = CoreStringType::deserialize(reader);
                return true;
            case hintPropertyKey:
                m_Hint = CoreStringType::deserialize(reader);
                return true;
            case headingLevelPropertyKey:
                m_HeadingLevel = CoreUintType::deserialize(reader);
                return true;
            case traitFlagsPropertyKey:
                m_TraitFlags = CoreUintType::deserialize(reader);
                return true;
            case stateFlagsPropertyKey:
                m_StateFlags = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void roleChanged() {}
    virtual void labelChanged() {}
    virtual void valueChanged() {}
    virtual void hintChanged() {}
    virtual void headingLevelChanged() {}
    virtual void traitFlagsChanged() {}
    virtual void stateFlagsChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/semantic/semantic_data_ext.inl"
#endif
};
} // namespace rive

#endif