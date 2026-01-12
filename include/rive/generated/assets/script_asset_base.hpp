#ifndef _RIVE_SCRIPT_ASSET_BASE_HPP_
#define _RIVE_SCRIPT_ASSET_BASE_HPP_
#include <string>
#include "rive/assets/file_asset.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class ScriptAssetBase : public FileAsset
{
protected:
    typedef FileAsset Super;

public:
    static const uint16_t typeKey = 529;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ScriptAssetBase::typeKey:
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
    static const uint16_t folderPathPropertyKey = 926;

protected:
    uint32_t m_GeneratorFunctionRef = 0;
    bool m_IsModule = false;
    std::string m_FolderPath = "";

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
        m_GeneratorFunctionRef = value;
        generatorFunctionRefChanged();
    }

    inline bool isModule() const { return m_IsModule; }
    void isModule(bool value)
    {
        if (m_IsModule == value)
        {
            return;
        }
        m_IsModule = value;
        isModuleChanged();
    }

    inline const std::string& folderPath() const { return m_FolderPath; }
    void folderPath(std::string value)
    {
        if (m_FolderPath == value)
        {
            return;
        }
        m_FolderPath = value;
        folderPathChanged();
    }

    Core* clone() const override;
    void copy(const ScriptAssetBase& object)
    {
        m_GeneratorFunctionRef = object.m_GeneratorFunctionRef;
        m_IsModule = object.m_IsModule;
        m_FolderPath = object.m_FolderPath;
        FileAsset::copy(object);
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
            case folderPathPropertyKey:
                m_FolderPath = CoreStringType::deserialize(reader);
                return true;
        }
        return FileAsset::deserialize(propertyKey, reader);
    }

protected:
    virtual void generatorFunctionRefChanged() {}
    virtual void isModuleChanged() {}
    virtual void folderPathChanged() {}
};
} // namespace rive

#endif