/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/refcnt.hpp"

namespace rive
{
class RenderImage;
};

namespace rive::gpu
{
class RenderTarget;
class RenderContextGLImpl;
class RenderContextWebGPUImpl;
class VulkanContext;
}; // namespace rive::gpu

namespace rive_tests
{
// Offscreen render target interface that can be accessed as a RenderImage or a
// RenderTarget.
// The backend handles barriers automatically.
class OffscreenRenderTarget : public rive::RefCnt<OffscreenRenderTarget>
{
public:
    virtual rive::RenderImage* asRenderImage() = 0;
    virtual rive::gpu::RenderTarget* asRenderTarget() = 0;
    virtual ~OffscreenRenderTarget() {}

    static rive::rcp<OffscreenRenderTarget> MakeGL(
        rive::gpu::RenderContextGLImpl*,
        uint32_t width,
        uint32_t height);

    static rive::rcp<OffscreenRenderTarget> MakeVulkan(
        rive::gpu::VulkanContext*,
        uint32_t width,
        uint32_t height,
        bool riveRenderable);

    static rive::rcp<OffscreenRenderTarget> MakeWebGPU(
        rive::gpu::RenderContextWebGPUImpl*,
        uint32_t width,
        uint32_t height);
};
}; // namespace rive_tests
