/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "ore_pipeline_gl.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include <string>
#include <vector>

namespace rive::ore
{

#if defined(ORE_BACKEND_GL)

PipelineGL::~PipelineGL()
{
  if (m_glProgram != 0)
  {
    glDeleteProgram(m_glProgram);
  }
}

static bool isShadowSampler(GLenum type)
{
  return type == GL_SAMPLER_2D_SHADOW || type == GL_SAMPLER_2D_ARRAY_SHADOW ||
         type == GL_SAMPLER_CUBE_SHADOW;
}

// The plain sampler types a depth texture can also be read through.
static bool isPlainDepthSampler(GLenum type)
{
  return type == GL_SAMPLER_2D || type == GL_SAMPLER_2D_ARRAY ||
         type == GL_SAMPLER_CUBE;
}

static uint32_t firstFreeTextureUnit(const Pipeline& pipeline)
{
  uint32_t free = 0;
  for (size_t i = 0; i < pipeline.m_bindingMap.size(); ++i)
  {
    const BindingMap::Entry& e = pipeline.m_bindingMap.at(i);
    if (slotSpace(e.kind) != SlotSpace::texture)
      continue;
    for (uint16_t slot : e.backendSlot)
    {
      if (slot != BindingMap::kAbsent && slot >= free)
        free = slot + 1u;
    }
  }
  return free;
}

namespace
{
struct SamplerUniform
{
  std::string name;
  bool shadow;
  GLint location = -1;
  GLint unit = 0;
};
} // namespace

bool PipelineGL::separateShadowUnits(std::string* error)
{
  const GLuint program = m_glProgram;
  GLint count = 0;
  glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);
  std::vector<SamplerUniform> uniforms;
  bool shadows = false;
  for (GLint i = 0; i < count; ++i)
  {
    char name[256] = {};
    GLint size = 0;
    GLenum type = 0;
    glGetActiveUniform(program, i, sizeof(name), nullptr, &size, &type, name);
    const bool shadow = isShadowSampler(type);
    if (size != 1 || (!shadow && !isPlainDepthSampler(type)))
      continue;
    uniforms.push_back({name, shadow});
    shadows |= shadow;
  }
  // Querying a uniform is a round trip on WebGL, so only where it can matter.
  if (!shadows)
    return true;
  // Emscripten resolves a uniform location against the current program, not
  // the one passed in, so the program is current before the first query.
  glUseProgram(program);
  for (SamplerUniform& uniform : uniforms)
  {
    uniform.location = glGetUniformLocation(program, uniform.name.c_str());
    glGetUniformiv(program, uniform.location, &uniform.unit);
  }

  GLint units = 0;
  glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &units);
  uint32_t next = firstFreeTextureUnit(*this);
  for (const SamplerUniform& shadow : uniforms)
  {
    const auto unit = static_cast<uint16_t>(shadow.unit);
    if (!shadow.shadow || comparesAt(unit))
      continue;
    m_shadowUnits.push_back(unit);
    bool moved = false;
    for (const SamplerUniform& plain : uniforms)
    {
      if (plain.shadow || plain.unit != shadow.unit)
        continue;
      if (next >= static_cast<uint32_t>(units))
      {
        glUseProgram(0);
        *error = "a depth texture read with and without a comparison needs "
                 "a second texture unit, past the " +
                 std::to_string(units) + " this GL context has";
        return false;
      }
      glUniform1i(plain.location, static_cast<GLint>(next));
      moved = true;
    }
    if (moved)
      m_unitTwins.push_back({unit, static_cast<uint16_t>(next++)});
  }
  glUseProgram(0);
  return true;
}

#endif // ORE_BACKEND_GL

} // namespace rive::ore
