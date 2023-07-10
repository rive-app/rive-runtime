/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/pls/gl/gles3.hpp"
#include <cstddef>

namespace glutils
{
// A nullptr for versionString will default to "#version 300 es\n".
void CompileAndAttachShader(GLuint program,
                            GLuint type,
                            const char* source,
                            const GLExtensions&,
                            const char* versionString = nullptr);

void CompileAndAttachShader(GLuint program,
                            GLuint type,
                            const char* defines[],
                            size_t numDefines,
                            const char* sources[],
                            size_t numSources,
                            const GLExtensions&,
                            const char* versionString = nullptr);

[[nodiscard]] GLuint CompileShader(GLuint type,
                                   const char* source,
                                   const GLExtensions&,
                                   const char* versionString = nullptr);

[[nodiscard]] GLuint CompileShader(GLuint type,
                                   const char* defines[],
                                   size_t numDefines,
                                   const char* sources[],
                                   size_t numSources,
                                   const GLExtensions&,
                                   const char* versionString = nullptr);

void LinkProgram(GLuint program);
} // namespace glutils
