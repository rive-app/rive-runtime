/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_shader_module.hpp"
#include "ore_shader_module_d3d11.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
namespace rive::ore
{

void rive::ore::ShaderModuleD3D11::ensureD3DShaders(ID3D11Device* device,
                                                    std::string* outError)
{
    std::call_once(m_d3dInitFlag,
                   [&]() { ensureD3DShadersImpl(device, outError); });
}

void ShaderModuleD3D11::ensureD3DShadersImpl(ID3D11Device* device,
                                             std::string* outError)
{
    if (m_hlslSource.empty())
        return;

    // Normalize line endings (SPIRV-Cross outputs \r\n on Windows).
    m_hlslSource.erase(
        std::remove(m_hlslSource.begin(), m_hlslSource.end(), '\r'),
        m_hlslSource.end());

    const char* target =
        (m_stage == ShaderStage::fragment) ? "ps_5_0" : "vs_5_0";
    const char* entry =
        m_hlslEntryPoint.empty() ? "main" : m_hlslEntryPoint.c_str();
    Microsoft::WRL::ComPtr<ID3DBlob> compiledBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    HRESULT hr = D3DCompile(m_hlslSource.c_str(),
                            m_hlslSource.size(),
                            nullptr,
                            nullptr,
                            nullptr,
                            entry,
                            target,
                            D3DCOMPILE_ENABLE_STRICTNESS |
                                D3DCOMPILE_OPTIMIZATION_LEVEL3,
                            0,
                            compiledBlob.GetAddressOf(),
                            errors.GetAddressOf());
    if (FAILED(hr))
    {
        if (outError)
        {
            const char* errMsg =
                errors ? static_cast<const char*>(errors->GetBufferPointer())
                       : "(no error log)";
            char buf[1024];
            snprintf(buf,
                     sizeof(buf),
                     "D3DCompile failed (entry=%s target=%s "
                     "hr=0x%08x): %s",
                     entry,
                     target,
                     static_cast<unsigned>(hr),
                     errMsg);
            *outError = buf;
        }
        return;
    }

    const void* code = compiledBlob->GetBufferPointer();
    SIZE_T codeSize = compiledBlob->GetBufferSize();
    // Store compiled bytecode for CreateInputLayout.
    m_bytecode.assign(static_cast<const uint8_t*>(code),
                      static_cast<const uint8_t*>(code) + codeSize);

    if (m_stage == ShaderStage::fragment)
    {
        device->CreatePixelShader(code,
                                  codeSize,
                                  nullptr,
                                  m_d3dPixelShader.GetAddressOf());
    }
    else if (m_stage == ShaderStage::vertex)
    {
        device->CreateVertexShader(code,
                                   codeSize,
                                   nullptr,
                                   m_d3dVertexShader.GetAddressOf());
    }
    else
    {
        HRESULT hr =
            device->CreateVertexShader(code,
                                       codeSize,
                                       nullptr,
                                       m_d3dVertexShader.GetAddressOf());
        if (FAILED(hr))
        {
            device->CreatePixelShader(code,
                                      codeSize,
                                      nullptr,
                                      m_d3dPixelShader.GetAddressOf());
        }
    }
}

} // namespace rive::ore
