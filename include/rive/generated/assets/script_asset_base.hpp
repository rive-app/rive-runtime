#ifndef _RIVE_SCRIPT_ASSET_BASE_HPP_
#define _RIVE_SCRIPT_ASSET_BASE_HPP_
#include "rive/assets/file_asset.hpp"
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

#ifdef WITH_RIVE_TOOLS
    static const uint16_t generatorFunctionRefPropertyKey = 893;
#endif

protected:
#ifdef WITH_RIVE_TOOLS
    uint32_t m_GeneratorFunctionRef = 0;
#endif

public:
#ifdef WITH_RIVE_TOOLS
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
#endif

    Core* clone() const override;
    void copy(const ScriptAssetBase& object)
    {
#ifdef WITH_RIVE_TOOLS
        m_GeneratorFunctionRef = object.m_GeneratorFunctionRef;
#endif
        FileAsset::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
#ifdef WITH_RIVE_TOOLS
            case generatorFunctionRefPropertyKey:
                m_GeneratorFunctionRef = CoreUintType::deserialize(reader);
                return true;
#endif
        }
        return FileAsset::deserialize(propertyKey, reader);
    }

protected:
#ifdef WITH_RIVE_TOOLS
    virtual void generatorFunctionRefChanged() {}
#endif
};
} // namespace rive

#endif