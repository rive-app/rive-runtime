/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_shader_module.hpp"
#include "ore_shader_module_d3d11.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
namespace rive::ore
{

void rive::ore::ShaderModuleD3D11::ensureD3DShaders(
    ID3D11Device* device,
    D3D11ShaderCache& cache,
    std::string* outError)
{
  std::call_once(m_d3dInitFlag, [&]() {
    ensureD3DShadersImpl(device, cache, outError);
  });
}

void ShaderModuleD3D11::ensureD3DShadersImpl(
    ID3D11Device* device,
    D3D11ShaderCache& cache,
    std::string* outError)
{
  if (m_hlslSource.empty())
    return;

  // Normalize line endings (SPIRV-Cross outputs \r\n on Windows).
  m_hlslSource.erase(
      std::remove(m_hlslSource.begin(), m_hlslSource.end(), '\r'),
      m_hlslSource.end());

  const char* target = (m_stage == ShaderStage::fragment) ? "ps_5_0" : "vs_5_0";
  const char* entry = m_hlslEntryPoint.empty() ? "main"
                                               : m_hlslEntryPoint.c_str();

  // Identical source compiles to identical bytecode on one device, so a module
  // whose source the context already compiled reuses that result instead of
  // paying D3DCompile again.
  const std::string cacheKey = oreD3DShaderCacheKey(
      target,
      entry,
      m_hlslSource);
  D3D11CompiledShader cached;
  if (cache.lookup(cacheKey, &cached))
  {
    m_bytecode = std::move(cached.bytecode);
    m_d3dVertexShader = std::move(cached.vertexShader);
    m_d3dPixelShader = std::move(cached.pixelShader);
    return;
  }

  Microsoft::WRL::ComPtr<ID3DBlob> compiledBlob;
  Microsoft::WRL::ComPtr<ID3DBlob> errors;
  HRESULT hr = D3DCompile(
      m_hlslSource.c_str(),
      m_hlslSource.size(),
      nullptr,
      nullptr,
      nullptr,
      entry,
      target,
      D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3,
      0,
      compiledBlob.GetAddressOf(),
      errors.GetAddressOf());
  if (FAILED(hr))
  {
    if (outError)
    {
      const char* errMsg = errors ? static_cast<const char*>(
                                        errors->GetBufferPointer())
                                  : "(no error log)";
      // Unbounded tail: the compiler log carries the line numbers the
      // author needs and overflows any fixed buffer.
      char prefix[128];
      snprintf(
          prefix,
          sizeof(prefix),
          "D3DCompile failed (entry=%s target=%s "
          "hr=0x%08x): ",
          entry,
          target,
          static_cast<unsigned>(hr));
      *outError = std::string(prefix) + errMsg;
    }
    return;
  }

  const void* code = compiledBlob->GetBufferPointer();
  SIZE_T codeSize = compiledBlob->GetBufferSize();
  // Store compiled bytecode for CreateInputLayout.
  m_bytecode.assign(
      static_cast<const uint8_t*>(code),
      static_cast<const uint8_t*>(code) + codeSize);

  HRESULT createHr;
  if (m_stage == ShaderStage::fragment)
  {
    createHr = device->CreatePixelShader(
        code,
        codeSize,
        nullptr,
        m_d3dPixelShader.GetAddressOf());
  }
  else if (m_stage == ShaderStage::vertex)
  {
    createHr = device->CreateVertexShader(
        code,
        codeSize,
        nullptr,
        m_d3dVertexShader.GetAddressOf());
  }
  else
  {
    createHr = device->CreateVertexShader(
        code,
        codeSize,
        nullptr,
        m_d3dVertexShader.GetAddressOf());
    if (FAILED(createHr))
    {
      createHr = device->CreatePixelShader(
          code,
          codeSize,
          nullptr,
          m_d3dPixelShader.GetAddressOf());
    }
  }

  // Only a usable result is worth sharing. A creation failure (memory
  // pressure, a lost device) leaves this module without a shader object as
  // before, and the next module with the same source compiles and tries
  // again instead of inheriting the failure for the cache's lifetime.
  if (SUCCEEDED(createHr))
  {
    cache.store(
        cacheKey,
        D3D11CompiledShader{m_bytecode, m_d3dVertexShader, m_d3dPixelShader},
        m_bytecode.size());
  }
}

} // namespace rive::ore
