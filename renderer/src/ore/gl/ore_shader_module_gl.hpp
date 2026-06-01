#pragma once
#include "rive/renderer/ore/ore_shader_module.hpp"

namespace rive::ore
{
class ContextGL;

class ShaderModuleGL : public LITE_RTTI_OVERRIDE(ShaderModule, ShaderModuleGL)
{
public:
    ShaderModuleGL() = default;
    ~ShaderModuleGL() override;

private:
    friend class ContextGL;
    unsigned int m_glShader = 0; // GLuint
    unsigned int m_glShaderType =
        0; // GLenum (GL_VERTEX_SHADER or GL_FRAGMENT_SHADER)
};
} // namespace rive::ore
