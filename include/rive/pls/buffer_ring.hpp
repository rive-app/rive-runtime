/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/pls/pls.hpp"

namespace rive::pls
{
// API-agnostic implementation of an abstract buffer ring. We use rings to ensure the GPU can render
// one frame in parallel while the CPU prepares the next frame.
//
// Calling mapBuffer() maps the next buffer in the ring.
//
// Calling unmapAndSubmitBuffer() submits the currently-mapped buffer for GPU rendering, in whatever
// way that is meaningful for the PLSRenderContext implementation.
//
// This class is meant to only be used through BufferRing<>.
class BufferRing
{
public:
    BufferRing(size_t capacity, size_t itemSizeInBytes) :
        m_capacity(capacity), m_itemSizeInBytes(itemSizeInBytes)
    {}
    virtual ~BufferRing() {}

    size_t capacity() const { return m_capacity; }
    size_t itemSizeInBytes() const { return m_itemSizeInBytes; }
    size_t totalSizeInBytes() const { return m_capacity * m_itemSizeInBytes * kBufferRingSize; }

    // Maps the next buffer in the ring.
    void* mapBuffer()
    {
        assert(!m_mapped);
        RIVE_DEBUG_CODE(m_mapped = true;)
        m_submittedBufferIdx = (m_submittedBufferIdx + 1) % kBufferRingSize;
        return onMapBuffer(m_submittedBufferIdx);
    }

    // Submits the currently-mapped buffer for GPU rendering, in whatever way that is meaningful for
    // the PLSRenderContext implementation.
    void unmapAndSubmitBuffer(size_t bytesWritten)
    {
        assert(m_mapped);
        RIVE_DEBUG_CODE(m_mapped = false;)
        onUnmapAndSubmitBuffer(m_submittedBufferIdx, bytesWritten);
    }

protected:
    int submittedBufferIdx() const
    {
        assert(!m_mapped);
        return m_submittedBufferIdx;
    }

    virtual void* onMapBuffer(int bufferIdx) = 0;
    virtual void onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten) = 0;

private:
    size_t m_capacity;
    size_t m_itemSizeInBytes;
    int m_submittedBufferIdx = 0;
    RIVE_DEBUG_CODE(bool m_mapped = false;)
};

// Buffer ring implementation for a GPU resource that doesn't support mapping. Mapping is emulated
// via shadow buffer on the CPU.
class BufferRingShadowImpl : public BufferRing
{
public:
    BufferRingShadowImpl(size_t capacity, size_t itemSizeInBytes) :
        BufferRing(capacity, itemSizeInBytes)
    {}
    ~BufferRingShadowImpl() override { free(m_shadowBuffer); }

    void* onMapBuffer(int bufferIdx) final
    {
        if (!m_shadowBuffer)
        {
            m_shadowBuffer = malloc(capacity() * itemSizeInBytes());
        }
        return m_shadowBuffer;
    }

    const void* shadowBuffer() const { return m_shadowBuffer; }

private:
    void* m_shadowBuffer = nullptr;
};

// BufferRingShadowImpl whose backing store is a 2D texture.
class TexelBufferRing : public BufferRingShadowImpl
{
public:
    enum class Format
    {
        rgba8,
        rgba32f,
        rgba32ui,
    };

    constexpr static size_t BytesPerPixel(Format format)
    {
        switch (format)
        {
            case Format::rgba8:
                return 4;
            case Format::rgba32f:
            case Format::rgba32ui:
                return 4 * 4;
        }
        RIVE_UNREACHABLE();
    }

    enum class Filter
    {
        nearest,
        linear,
    };

    TexelBufferRing(Format format, size_t widthInItems, size_t height, size_t texelsPerItem) :
        BufferRingShadowImpl(height * widthInItems, texelsPerItem * BytesPerPixel(format)),
        m_format(format),
        m_widthInItems(widthInItems),
        m_height(height),
        m_texelsPerItem(texelsPerItem)
    {}

    size_t widthInItems() const { return m_widthInItems; }
    size_t widthInTexels() const { return m_widthInItems * m_texelsPerItem; }
    size_t height() const { return m_height; }

protected:
    void onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten) final
    {
        size_t texelsWritten = bytesWritten / BytesPerPixel(m_format);
        assert(texelsWritten * BytesPerPixel(m_format) == bytesWritten);
        size_t updateWidthInTexels = std::min(texelsWritten, widthInTexels());
        size_t updateHeight = (texelsWritten + widthInTexels() - 1) / widthInTexels();
        submitTexels(bufferIdx, updateWidthInTexels, updateHeight);
    }

    virtual void submitTexels(int textureIdx, size_t updateWidthInTexels, size_t updateHeight) = 0;

    const Format m_format;
    const size_t m_widthInItems;
    const size_t m_height;
    const size_t m_texelsPerItem;
};

// BufferRingShadowImpl that resides solely in CPU memory, and therefore doesn't require a ring.
class HeapBufferRing : public BufferRing
{
public:
    HeapBufferRing(size_t capacity, size_t itemSizeInBytes) :
        BufferRing(capacity, itemSizeInBytes), m_contents(new uint8_t[capacity * itemSizeInBytes])
    {}

    const void* contents() const { return m_contents.get(); }
    void* contents() { return m_contents.get(); }

protected:
    void* onMapBuffer(int bufferIdx) override { return contents(); }
    void onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten) override {}

private:
    std::unique_ptr<uint8_t[]> m_contents;
};
} // namespace rive::pls
