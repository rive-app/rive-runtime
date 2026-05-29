#include "rive/assets/shader_asset.hpp"
#include "rive/signed_content_header.hpp"

using namespace rive;

bool rive::ShaderAsset::decode(Span<uint8_t> data, Factory* factory)
{
    // `data` is always a SignedContentHeader envelope
    // (`[flags:1][sig:64?][RSTB...]`) shared with ScriptAsset. Raw-RSTB
    // callers (editor live-preview) must prepend a single 0x00 flag byte.
    SignedContentHeader envelope(std::move(data));
    if (!envelope.isValid())
    {
        return false;
    }
    auto inner = envelope.content();
    m_bytes = SimpleArray<uint8_t>(inner.data(), inner.size());
    m_index.clear();
    m_pairs.clear();

    const uint8_t* p = m_bytes.data();
    const uint8_t* end = p + m_bytes.size();

    // Header: magic(u32LE) + version(u16LE) + variant_count(u8) +
    //         section_count(u8)
    if (m_bytes.size() < 8)
    {
        return false;
    }
    uint32_t magic = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    if (magic != 0x52535442u) // "RSTB"
    {
        return false;
    }
    uint16_t version = (uint16_t)(p[4] | (p[5] << 8));
    if (version != 3)
    {
        return false;
    }
    uint8_t variantCount = p[6];
    uint8_t sectionCount = p[7];
    p += 8;

    // VariantDescriptor: target(u8) + blob_offset(u32) + blob_size(u32) = 9
    constexpr size_t kVariantDescriptorSize = 9;
    if (p + (size_t)variantCount * kVariantDescriptorSize > end)
    {
        return false;
    }
    for (uint8_t v = 0; v < variantCount; v++)
    {
        uint8_t target = p[0];
        uint32_t blobOffset = (uint32_t)p[1] | ((uint32_t)p[2] << 8) |
                              ((uint32_t)p[3] << 16) | ((uint32_t)p[4] << 24);
        uint32_t blobSize = (uint32_t)p[5] | ((uint32_t)p[6] << 8) |
                            ((uint32_t)p[7] << 16) | ((uint32_t)p[8] << 24);
        p += kVariantDescriptorSize;
        m_index[target] = {blobOffset, blobSize};
    }

    // Tagged metadata sections.
    for (uint8_t sec = 0; sec < sectionCount; sec++)
    {
        if (p + 3 > end)
        {
            return false;
        }
        uint8_t tag = p[0];
        uint16_t length = (uint16_t)(p[1] | (p[2] << 8));
        p += 3;
        if (p + length > end)
        {
            return false;
        }

        if (tag == 1 && length >= 1)
        {
            // Tag 1: texture-sampler pairs.
            uint8_t pairCount = p[0];
            if (length >= 1 + (size_t)pairCount * 4)
            {
                const uint8_t* pd = p + 1;
                for (uint8_t i = 0; i < pairCount; i++)
                {
                    m_pairs.push_back({pd[0], pd[1], pd[2], pd[3]});
                    pd += 4;
                }
            }
        }
        // Unknown tags: skip.
        p += length;
    }

    // p now points to the start of the blob data section.
    size_t blobDataStart = (size_t)(p - m_bytes.data());

    // Fix up offsets to absolute and validate that each variant's range
    // stays within m_bytes. Arithmetic widened to size_t so an adversarial
    // (offset, size) pair can't overflow uint32_t past the bounds check.
    for (auto& kv : m_index)
    {
        size_t absOffset = (size_t)kv.second.offset + blobDataStart;
        if (absOffset > m_bytes.size() ||
            (size_t)kv.second.size > m_bytes.size() - absOffset)
        {
            m_index.clear();
            return false;
        }
        kv.second.offset = (uint32_t)absOffset;
    }

    return true;
}

Span<const uint8_t> ShaderAsset::findShader(uint8_t target) const
{
    auto it = m_index.find(target);
    if (it == m_index.end())
    {
        return {};
    }
    const auto& v = it->second;
    return Span<const uint8_t>(m_bytes.data() + v.offset, v.size);
}
