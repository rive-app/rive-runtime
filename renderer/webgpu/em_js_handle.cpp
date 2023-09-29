/*
 * Copyright 2023 Rive
 */

#include "rive/pls/webgpu/em_js_handle.hpp"

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
#endif

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
