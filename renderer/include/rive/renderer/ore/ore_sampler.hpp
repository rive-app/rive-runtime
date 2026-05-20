/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "utils/lite_rtti.hpp"
#include "rive/renderer/ore/ore_types.hpp"

namespace rive::ore
{

class Context;

class Sampler : public RefCnt<Sampler>, public ENABLE_LITE_RTTI(Sampler)
{
public:
    virtual ~Sampler() = default;

    // Default: immediately free. Backends that need deferred GPU-resource
    // destruction (D3D12, Vulkan) override this.
    virtual void onRefCntReachedZero() const { delete this; }

protected:
    friend class Context;
    friend class RenderPass;

    Sampler() = default;
};

} // namespace rive::ore
