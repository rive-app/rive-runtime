/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/pls/pls.hpp"
#include "rive/pls/pls_render_target.hpp"

#ifndef RIVE_OBJC_NOP
#import <Metal/Metal.h>
#endif

namespace rive::pls
{
// Metal backend implementation of PLSRenderTarget.
class PLSRenderTargetMetal : public PLSRenderTarget
{
public:
    ~PLSRenderTargetMetal() override {}

    MTLPixelFormat pixelFormat() const { return m_pixelFormat; }

    void setTargetTexture(id<MTLTexture> texture);
    id<MTLTexture> targetTexture() const { return m_targetTexture; }

private:
    friend class PLSRenderContextMetal;

    PLSRenderTargetMetal(id<MTLDevice> gpu,
                         MTLPixelFormat pixelFormat,
                         size_t width,
                         size_t height,
                         const PlatformFeatures&);

    const MTLPixelFormat m_pixelFormat;
    id<MTLTexture> m_targetTexture;
    id<MTLTexture> m_coverageMemorylessTexture;
    id<MTLTexture> m_originalDstColorMemorylessTexture;
    id<MTLTexture> m_clipMemorylessTexture;
};
} // namespace rive::pls
