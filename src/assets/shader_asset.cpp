#include "rive/assets/shader_asset.hpp"
#include "rive/signed_content_header.hpp"

using namespace rive;

bool ShaderAsset::decode(SimpleArray<uint8_t>& data, Factory*)
{
    // `data` is always a SignedContentHeader envelope
    // (`[flags:1][sig:64?][RSTB...]`) shared with ScriptAsset. Callers that
    // start from a raw RSTB blob (e.g. the editor's live-preview path in
    // lua_gpu.cpp::makeShaderFromRstb) must prepend the unsigned envelope
    // (a single 0x00 flag byte) before calling decode so there's a single
    // canonical input format here.
    SignedContentHeader envelope(Span<const uint8_t>(data.data(), data.size()));
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

    // Validate header: magic(u32LE) + version(u16LE) + shader_count(u16LE)
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
    if (version != 2)
    {
        return false;
    }
    uint16_t shaderCount = (uint16_t)(p[6] | (p[7] << 8));
    p += 8;

    // Parse shader entries; collect raw (blobOffset, blobSize) values.
    // blob_offset is relative to the blob data section which starts right
    // after all entries and tagged sections.
    for (uint16_t s = 0; s < shaderCount; s++)
    {
        // shader_id(u32) + variant_count(u8) + section_count(u8)
        constexpr size_t kEntryHeaderSize = 6;
        if (p + kEntryHeaderSize > end)
        {
            return false;
        }
        uint32_t shaderId = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                            ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        uint8_t variantCount = p[4];
        uint8_t sectionCount = p[5];
        p += kEntryHeaderSize;

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
                                  ((uint32_t)p[3] << 16) |
                                  ((uint32_t)p[4] << 24);
            uint32_t blobSize = (uint32_t)p[5] | ((uint32_t)p[6] << 8) |
                                ((uint32_t)p[7] << 16) | ((uint32_t)p[8] << 24);
            p += kVariantDescriptorSize;

            uint64_t key = ((uint64_t)shaderId << 8) | target;
            m_index[key] = {blobOffset, blobSize};
        }

        // Parse tagged metadata sections (v2+).
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
    }

    // p now points to the start of the blob data section.
    uint32_t blobDataStart = (uint32_t)(p - m_bytes.data());

    // Fix up all offsets to be absolute.
    for (auto& kv : m_index)
    {
        kv.second.offset += blobDataStart;
    }

    return true;
}

Span<const uint8_t> ShaderAsset::findShader(uint32_t shaderId,
                                            uint8_t target) const
{
    uint64_t key = ((uint64_t)shaderId << 8) | target;
    auto it = m_index.find(key);
    if (it == m_index.end())
    {
        return {};
    }
    const auto& v = it->second;
    if ((size_t)(v.offset + v.size) > m_bytes.size())
    {
        return {};
    }
    return Span<const uint8_t>(m_bytes.data() + v.offset, v.size);
}
