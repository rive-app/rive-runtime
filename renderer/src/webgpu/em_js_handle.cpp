/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/webgpu/em_js_handle.hpp"

EmJsHandle& EmJsHandle::operator=(EmJsHandle&& rhs)
{
    int tmp = rhs.m_handle;
    rhs.m_handle = this->m_handle;
    this->m_handle = tmp;
    return *this;
}

#ifdef RIVE_DAWN
EmJsHandle::~EmJsHandle() { assert(m_handle == 0); }

wgpu::ShaderModule EmJsHandle::compileShaderModule(wgpu::Device device,
                                                   const char* language,
                                                   const char* source)
{
    RIVE_UNREACHABLE();
}

wgpu::ShaderModule EmJsHandle::compileSPIRVShaderModule(wgpu::Device device,
                                                        const uint32_t* code,
                                                        uint32_t codeSize)
{
    wgpu::ShaderModuleSPIRVDescriptor spirvDesc;
    spirvDesc.code = code;
    spirvDesc.codeSize = codeSize;
    wgpu::ShaderModuleDescriptor descriptor;
    descriptor.nextInChain = &spirvDesc;
    return device.CreateShaderModule(&descriptor);
}
#endif

#ifdef RIVE_WEBGPU
EmJsHandle::~EmJsHandle()
{
    if (m_handle != 0)
    {
        emscripten_webgpu_release_js_handle(m_handle);
    }
}

EM_JS(int, compile_glsl_shader_module_js, (int device, const char* language, const char* source), {
    device = JsValStore.get(device);
    language = UTF8ToString(language);
    source = UTF8ToString(source);
    const shader = device.createShaderModule({
        language : language,
        code : source,
    });
    return JsValStore.add(shader);
});

wgpu::ShaderModule EmJsHandle::compileShaderModule(wgpu::Device device,
                                                   const char* language,
                                                   const char* source)
{
    m_handle = compile_glsl_shader_module_js(emscripten_webgpu_export_device(device.Get()),
                                             source,
                                             language);
    return wgpu::ShaderModule::Acquire(emscripten_webgpu_import_shader_module(m_handle));
}

EM_JS(int, compile_spirv_shader_module_js, (int device, uintptr_t indexU32, uint32_t codeSize), {
    device = JsValStore.get(device);
    // Copy data off the WASM heap before sending it to WebGPU bindings.
    const code = new Uint32Array(codeSize);
    code.set(Module.HEAPU32.subarray(indexU32, indexU32 + codeSize));
    const shader = device.createShaderModule({
        language : "spirv",
        code : code,
    });
    return JsValStore.add(shader);
});

wgpu::ShaderModule EmJsHandle::compileSPIRVShaderModule(wgpu::Device device,
                                                        const uint32_t* code,
                                                        uint32_t codeSize)
{
    assert(reinterpret_cast<uintptr_t>(code) % sizeof(uint32_t) == 0);
    m_handle = compile_spirv_shader_module_js(emscripten_webgpu_export_device(device.Get()),
                                              reinterpret_cast<uintptr_t>(code) / sizeof(uint32_t),
                                              codeSize);
    return wgpu::ShaderModule::Acquire(emscripten_webgpu_import_shader_module(m_handle));
}
#endif
