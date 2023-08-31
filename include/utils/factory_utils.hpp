/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_FACTORY_UTILS_HPP_
#define _RIVE_FACTORY_UTILS_HPP_

#include "rive/factory.hpp"

namespace rive
{

// Generic subclass of RenderBuffer that just stores the data on the cpu.
//
class DataRenderBuffer : public RenderBuffer
{
public:
    DataRenderBuffer(RenderBufferType type, RenderBufferFlags flags, size_t sizeInBytes) :
        RenderBuffer(type, flags, sizeInBytes)
    {
        m_storage = malloc(sizeInBytes);
    }

    ~DataRenderBuffer() { free(m_storage); }

    const float* f32s() const { return reinterpret_cast<const float*>(m_storage); }

    const uint16_t* u16s() const { return reinterpret_cast<const uint16_t*>(m_storage); }

    const Vec2D* vecs() const { return reinterpret_cast<const Vec2D*>(f32s()); }

    static const DataRenderBuffer* Cast(const RenderBuffer* buffer)
    {
        return static_cast<const DataRenderBuffer*>(buffer);
    }

protected:
    void* onMap() override { return m_storage; }
    void onUnmap() override {}

private:
    void* m_storage;
};

} // namespace rive

#endif
