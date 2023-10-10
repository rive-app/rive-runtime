/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/rive_types.hpp"

#ifdef RIVE_DAWN
#include <dawn/webgpu_cpp.h>
#endif

#ifdef RIVE_WEBGPU
#include <webgpu/webgpu_cpp.h>
#include <emscripten.h>
#include <emscripten/html5_webgpu.h>
#endif

// RAII object that stores and releases a handle to a WebGPU object passed from Javascript.
// Original source:
// https://github.com/emscripten-core/emscripten/blob/08cca043d8ba313d774bec8153fab66b70a6fe60/test/webgpu_jsvalstore.cpp
class EmJsHandle
{
public:
    EmJsHandle() : m_handle(0) {}
    explicit EmJsHandle(int handle) : m_handle(handle) {}
    ~EmJsHandle();

    EmJsHandle(const EmJsHandle&) = delete;
    EmJsHandle& operator=(const EmJsHandle&) = delete;

    EmJsHandle(EmJsHandle&& rhs) : m_handle(rhs.m_handle) { rhs.m_handle = 0; }
    EmJsHandle& operator=(EmJsHandle&& rhs);

    int get() const { return m_handle; }

    wgpu::ShaderModule compileSPIRVShaderModule(wgpu::Device device,
                                                const uint32_t* code,
                                                uint32_t codeSize);

    wgpu::ShaderModule compileShaderModule(wgpu::Device device,
                                           const char* language,
                                           const char* source);

private:
    int m_handle;
};
