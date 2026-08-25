#ifndef _RIVE_VIEW_MODEL_INSTANCE_LIST_ITEM_BASE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_LIST_ITEM_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class ViewModelInstanceListItemBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 427;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelInstanceListItemBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t viewModelIdPropertyKey = 549;
    static const uint16_t viewModelInstanceIdPropertyKey = 550;

protected:
    Id m_ViewModelId = kEmptyId;
    Id m_ViewModelInstanceId = kEmptyId;

public:
    inline Id viewModelId() const { return m_ViewModelId; }
    void viewModelId(Id value)
    {
        if (m_ViewModelId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(viewModelIdPropertyKey, &m_ViewModelId, &value);
        m_ViewModelId = value;
        RIVE_EDITOR_CHANGED(viewModelIdChanged());
        notifyPropertyChanged(viewModelIdPropertyKey);
    }

    inline Id viewModelInstanceId() const { return m_ViewModelInstanceId; }
    void viewModelInstanceId(Id value)
    {
        if (m_ViewModelInstanceId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(viewModelInstanceIdPropertyKey,
                             &m_ViewModelInstanceId,
                             &value);
        m_ViewModelInstanceId = value;
        RIVE_EDITOR_CHANGED(viewModelInstanceIdChanged());
        notifyPropertyChanged(viewModelInstanceIdPropertyKey);
    }

    Core* clone() const override;
    void copy(const ViewModelInstanceListItemBase& object)
    {
        m_ViewModelId = object.m_ViewModelId;
        m_ViewModelInstanceId = object.m_ViewModelInstanceId;
        RIVE_EDITOR_COPY(object);
        RIVE_EDITOR_COPY_VALIDATED(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case viewModelIdPropertyKey:
                m_ViewModelId = CoreIdType::runtimeDeserialize(reader);
                return true;
            case viewModelInstanceIdPropertyKey:
                m_ViewModelInstanceId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return false;
    }

protected:
    virtual void viewModelIdChanged() {}
    virtual void viewModelInstanceIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/viewmodel/viewmodel_instance_list_item_ext.inl"
#endif
};
} // namespace rive

#endif