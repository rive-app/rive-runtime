/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/gpu_resource.hpp"
#include "utils/lite_rtti.hpp"
#include "rive/renderer/ore/ore_types.hpp"

namespace rive::ore
{

class Context;

class Sampler : public rive::gpu::GPUResource, public ENABLE_LITE_RTTI(Sampler)
{
public:
    virtual ~Sampler() = default;

protected:
    friend class Context;
    friend class RenderPass;

    Sampler() : rive::gpu::GPUResource(nullptr) {}
    Sampler(rcp<rive::gpu::GPUResourceManager> manager) :
        rive::gpu::GPUResource(std::move(manager))
    {}
};

} // namespace rive::ore
