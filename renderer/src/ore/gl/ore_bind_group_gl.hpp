#pragma once
#include "rive/renderer/ore/ore_bind_group.hpp"
#include <cstdint>
#include <vector>

namespace rive::ore
{
class ContextGL;
class RenderPassGL;

class BindGroupGL : public LITE_RTTI_OVERRIDE(BindGroup, BindGroupGL)
{
public:
    struct GLUBOBinding
    {
        unsigned int buffer = 0; // GLuint
        uint32_t offset = 0;
        uint32_t size = 0;
        uint32_t binding = 0; // WGSL @binding, for sort
        uint32_t slot = 0;    // GL UBO binding point
        bool hasDynamicOffset = false;
    };
    struct GLTexBinding
    {
        unsigned int texture = 0; // GLuint
        unsigned int target = 0;  // GLenum (GL_TEXTURE_2D etc.)
        uint32_t slot = 0;        // texture unit
    };
    struct GLSamplerBinding
    {
        unsigned int sampler = 0; // GLuint
        uint32_t slot = 0;        // sampler unit
    };

    BindGroupGL() = default;
    ~BindGroupGL() override = default;

private:
    friend class ContextGL;
    friend class RenderPassGL;
    std::vector<GLUBOBinding> m_glUBOs;
    std::vector<GLTexBinding> m_glTextures;
    std::vector<GLSamplerBinding> m_glSamplers;
};
} // namespace rive::ore
