/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/pls/gl/gles3.hpp"
#include <cstddef>

namespace glutils
{
void CompileAndAttachShader(GLuint program, GLuint type, const char* source);
void CompileAndAttachShader(GLuint program, GLuint type, const char* sources[], size_t n);
[[nodiscard]] GLuint CompileShader(GLuint type, const char* source);
[[nodiscard]] GLuint CompileShader(GLuint type, const char* sources[], size_t n);
void LinkProgram(GLuint program);
} // namespace glutils
