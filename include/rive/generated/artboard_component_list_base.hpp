#ifndef _RIVE_ARTBOARD_COMPONENT_LIST_BASE_HPP_
#define _RIVE_ARTBOARD_COMPONENT_LIST_BASE_HPP_
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#include "rive/drawable.hpp"
namespace rive
{
class ArtboardComponentListBase : public Drawable
{
protected:
    typedef Drawable Super;

public:
    static const uint16_t typeKey = 559;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ArtboardComponentListBase::typeKey:
            case DrawableBase::typeKey:
            case NodeBase::typeKey:
            case TransformComponentBase::typeKey:
            case WorldTransformComponentBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t listSourcePropertyKey = 800;

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
    void copy(const ArtboardComponentListBase& object)
    {
        m_ListSource = object.m_ListSource;
        Drawable::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case listSourcePropertyKey:
                m_ListSource = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return Drawable::deserialize(propertyKey, reader);
    }

protected:
    virtual void listSourceChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/artboard_component_list_ext.inl"
#endif
};
} // namespace rive

#endif