/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_FACTORY_UTILS_HPP_
#define _RIVE_FACTORY_UTILS_HPP_

#include "rive/factory.hpp"

namespace rive {

    // Generic subclass of RenderBuffer that just stores the data on the cpu.
    //
    class DataRenderBuffer : public RenderBuffer {
        const size_t m_elemSize;
        std::vector<uint32_t> m_storage; // store 32bits for alignment

    public:
        DataRenderBuffer(const void* src, size_t count, size_t elemSize) :
            RenderBuffer(count), m_elemSize(elemSize) {
            const size_t bytes = count * elemSize;
            m_storage.resize((bytes + 3) >> 2); // round up to next 32bit count
            memcpy(m_storage.data(), src, bytes);
        }

        const float* f32s() const {
            assert(m_elemSize == sizeof(float));
            return reinterpret_cast<const float*>(m_storage.data());
        }

        const uint16_t* u16s() const {
            assert(m_elemSize == sizeof(uint16_t));
            return reinterpret_cast<const uint16_t*>(m_storage.data());
        }

        const Vec2D* vecs() const { return reinterpret_cast<const Vec2D*>(this->f32s()); }

        size_t elemSize() const { return m_elemSize; }

        static const DataRenderBuffer* Cast(const RenderBuffer* buffer) {
            return static_cast<const DataRenderBuffer*>(buffer);
        }

        template <typename T> static rcp<RenderBuffer> Make(Span<T> span) {
            return rcp<RenderBuffer>(new DataRenderBuffer(span.data(), span.size(), sizeof(T)));
        }
    };

} // namespace rive

#endif
