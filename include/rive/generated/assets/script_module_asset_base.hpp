#ifndef _RIVE_SCRIPT_MODULE_ASSET_BASE_HPP_
#define _RIVE_SCRIPT_MODULE_ASSET_BASE_HPP_
#include "rive/assets/file_asset.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class ScriptModuleAssetBase : public FileAsset
{
protected:
    typedef FileAsset Super;

public:
    static const uint16_t typeKey = 1071;

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t languagePropertyKey = 1087;

protected:
    uint32_t m_Language = 0;

public:
    inline uint32_t language() const { return m_Language; }
    void language(uint32_t value)
    {
        if (m_Language == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(languagePropertyKey, &m_Language, &value);
        m_Language = value;
        notifyPropertyChanged(languagePropertyKey);
    }

    Core* clone() const override;
    void copy(const ScriptModuleAssetBase& object)
    {
        m_Language = object.m_Language;
        FileAsset::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case languagePropertyKey:
                m_Language = CoreUintType::deserialize(reader);
                return true;
        }
        return FileAsset::deserialize(propertyKey, reader);
    }

protected:
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/assets/script_module_asset_ext.inl"
#endif
};
} // namespace rive

#endif