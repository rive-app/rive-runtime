#ifndef _RIVE_VIEW_MODEL_INSTANCE_LIST_BASE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_LIST_BASE_HPP_
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
namespace rive
{
class ViewModelInstanceListBase : public ViewModelInstanceValue
{
protected:
    typedef ViewModelInstanceValue Super;

public:
    static const uint16_t typeKey = 441;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelInstanceListBase::typeKey:
            case ViewModelInstanceValueBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t listSourcePropertyKey = 966;

protected:
    Id m_ListSource = kEmptyId;

public:
    inline Id listSource() const { return m_ListSource; }
    void listSource(Id value)
    {
        if (m_ListSource == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(listSourcePropertyKey, &m_ListSource, &value);
        m_ListSource = value;
        RIVE_EDITOR_CHANGED(listSourceChanged());
        notifyPropertyChanged(listSourcePropertyKey);
    }

    Core* clone() const override;
    void copy(const ViewModelInstanceListBase& object)
    {
        m_ListSource = object.m_ListSource;
        ViewModelInstanceValue::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case listSourcePropertyKey:
                m_ListSource = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return ViewModelInstanceValue::deserialize(propertyKey, reader);
    }

protected:
    virtual void listSourceChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/viewmodel/viewmodel_instance_list_ext.inl"
#endif
};
} // namespace rive

#endif