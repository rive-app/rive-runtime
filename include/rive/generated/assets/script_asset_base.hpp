#ifndef _RIVE_SCRIPT_ASSET_BASE_HPP_
#define _RIVE_SCRIPT_ASSET_BASE_HPP_
#include "rive/assets/text_asset.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class ScriptAssetBase : public TextAsset
{
protected:
    typedef TextAsset Super;

public:
    static const uint16_t typeKey = 529;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ScriptAssetBase::typeKey:
            case TextAssetBase::typeKey:
            case FileAssetBase::typeKey:
            case AssetBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t generatorFunctionRefPropertyKey = 893;
    static const uint16_t isModulePropertyKey = 914;
    static const uint16_t serializedImplementedMethodsPropertyKey = 1022;

protected:
    uint32_t m_GeneratorFunctionRef = 0;
    bool m_IsModule = false;
    uint32_t m_SerializedImplementedMethods = 2097151;

public:
    inline uint32_t generatorFunctionRef() const
    {
        return m_GeneratorFunctionRef;
    }
    void generatorFunctionRef(uint32_t value)
    {
        if (m_GeneratorFunctionRef == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(generatorFunctionRefPropertyKey,
                             &m_GeneratorFunctionRef,
                             &value);
        m_GeneratorFunctionRef = value;
        RIVE_EDITOR_CHANGED(generatorFunctionRefChanged());
        notifyPropertyChanged(generatorFunctionRefPropertyKey);
    }

    inline bool isModule() const { return m_IsModule; }
    void isModule(bool value)
    {
        if (m_IsModule == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(isModulePropertyKey, &m_IsModule, &value);
        m_IsModule = value;
        RIVE_EDITOR_CHANGED(isModuleChanged());
        notifyPropertyChanged(isModulePropertyKey);
    }

    inline uint32_t serializedImplementedMethods() const
    {
        return m_SerializedImplementedMethods;
    }
    void serializedImplementedMethods(uint32_t value)
    {
        if (m_SerializedImplementedMethods == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(serializedImplementedMethodsPropertyKey,
                             &m_SerializedImplementedMethods,
                             &value);
        m_SerializedImplementedMethods = value;
        RIVE_EDITOR_CHANGED(serializedImplementedMethodsChanged());
        notifyPropertyChanged(serializedImplementedMethodsPropertyKey);
    }

    Core* clone() const override;
    void copy(const ScriptAssetBase& object)
    {
        m_GeneratorFunctionRef = object.m_GeneratorFunctionRef;
        m_IsModule = object.m_IsModule;
        m_SerializedImplementedMethods = object.m_SerializedImplementedMethods;
        RIVE_EDITOR_COPY(object);
        TextAsset::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case generatorFunctionRefPropertyKey:
                m_GeneratorFunctionRef = CoreUintType::deserialize(reader);
                return true;
            case isModulePropertyKey:
                m_IsModule = CoreBoolType::deserialize(reader);
                return true;
            case serializedImplementedMethodsPropertyKey:
                m_SerializedImplementedMethods =
                    CoreUintType::deserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return TextAsset::deserialize(propertyKey, reader);
    }

protected:
    virtual void generatorFunctionRefChanged() {}
    virtual void isModuleChanged() {}
    virtual void serializedImplementedMethodsChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/assets/script_asset_ext.inl"
#endif
};
} // namespace rive

#endif