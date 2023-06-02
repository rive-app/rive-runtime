/*
 * Copyright 2023 Rive
 */

// This file includes the Metal Objective-C backend's headers in a way that the obfuscator can find
// their symbols without needing to support Objective-C. We currently use castxml, which does not
// support Objective-C.

#define RIVE_OBJC_NOP

template <typename T> struct id
{};

using MTLDevice = void*;
using MTLCommandQueue = void*;
using MTLLibrary = void*;
using MTLTexture = void*;
using MTLBuffer = void*;
using MTLPixelFormat = void*;
using MTLTextureUsage = void*;

#include "rive/pls/metal/pls_render_context_metal.h"
#include "rive/pls/metal/pls_render_target_metal.h"
#include "buffer_ring_metal.h"

namespace rive::pls
{
const char* rive_pls_macosx_metallib;
int rive_pls_macosx_metallib_len;
const char* rive_pls_ios_metallib;
int rive_pls_ios_metallib_len;
void make_memoryless_pls_texture();
void make_pipeline_state();
} // namespace rive::pls
