#pragma once
#include "rive/renderer/ore/ore_shader_module.hpp"
#import <Metal/Metal.h>

namespace rive::ore
{
class ContextMetal;

class ShaderModuleMetal
    : public LITE_RTTI_OVERRIDE(ShaderModule, ShaderModuleMetal)
{
public:
    ShaderModuleMetal() = default;
    ~ShaderModuleMetal() override = default; // ARC releases m_mtlLibrary

private:
    friend class ContextMetal;
    id<MTLLibrary> m_mtlLibrary = nil;
};
} // namespace rive::ore
