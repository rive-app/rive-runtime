#pragma once
#include "rive/renderer/ore/ore_sampler.hpp"
#import <Metal/Metal.h>

namespace rive::ore
{
class ContextMetal;

class SamplerMetal : public LITE_RTTI_OVERRIDE(Sampler, SamplerMetal)
{
public:
    SamplerMetal() = default;
    ~SamplerMetal() override = default; // ARC releases m_mtlSampler

private:
    friend class ContextMetal;
    id<MTLSamplerState> m_mtlSampler = nil;
};
} // namespace rive::ore
