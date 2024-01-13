/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/pls/gl/gles3.hpp"
#include <cstddef>
#include <string>

namespace glutils
{
void CompileAndAttachShader(GLuint program, GLuint type, const char* source, const GLCapabilities&);

void CompileAndAttachShader(GLuint program,
                            GLuint type,
                            const char* defines[],
                            size_t numDefines,
                            const char* sources[],
                            size_t numSources,
                            const GLCapabilities&);

[[nodiscard]] GLuint CompileShader(GLuint type, const char* source, const GLCapabilities&);

[[nodiscard]] GLuint CompileShader(GLuint type,
                                   const char* defines[],
                                   size_t numDefines,
                                   const char* sources[],
                                   size_t numSources,
                                   const GLCapabilities&);

[[nodiscard]] GLuint CompileRawGLSL(GLuint shaderType, const char* rawGLSL);

void LinkProgram(GLuint program);
} // namespace glutils
