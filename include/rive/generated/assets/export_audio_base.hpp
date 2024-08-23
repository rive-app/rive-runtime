#ifndef _RIVE_EXPORT_AUDIO_BASE_HPP_
#define _RIVE_EXPORT_AUDIO_BASE_HPP_
#include "rive/assets/file_asset.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class ExportAudioBase : public FileAsset
{
protected:
    typedef FileAsset Super;

public:
    static const uint16_t typeKey = 422;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ExportAudioBase::typeKey:
            case FileAssetBase::typeKey:
            case AssetBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t volumePropertyKey = 530;

private:
    float m_Volume = 1.0f;

public:
    inline float volume() const { return m_Volume; }
    void volume(float value)
    {
        if (m_Volume == value)
        {
            return;
        }
        m_Volume = value;
        volumeChanged();
    }

    void copy(const ExportAudioBase& object)
    {
        m_Volume = object.m_Volume;
        FileAsset::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case volumePropertyKey:
                m_Volume = CoreDoubleType::deserialize(reader);
                return true;
        }
        return FileAsset::deserialize(propertyKey, reader);
    }

protected:
    virtual void volumeChanged() {}
};
} // namespace rive

#endif