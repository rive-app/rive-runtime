#pragma once
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_types.hpp"
#include "../ore_d3d_shader_cache.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <mutex>
#include <vector>

namespace rive::ore
{
class ContextD3D11;

// One D3DCompile result and the device objects made from it. Shader objects
// are immutable and reference counted, so modules compiled from the same
// source share them.
struct D3D11CompiledShader
{
  std::vector<uint8_t> bytecode;
  Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
};

// Owned by the ContextD3D11 whose device compiled the entries.
class D3D11ShaderCache : public D3DShaderCache<D3D11CompiledShader>
{};

class ShaderModuleD3D11
    : public LITE_RTTI_OVERRIDE(ShaderModule, ShaderModuleD3D11)
{
public:
  ShaderModuleD3D11() = default;
  ~ShaderModuleD3D11() override = default; // ComPtrs released automatically

  // Lazily compiles m_hlslSource into m_d3dVertexShader / m_d3dPixelShader
  // on the first pipeline build. AMD drivers crash on cross-process DXBC,
  // so compilation is deferred until a device context is available.
  void ensureD3DShaders(
      ID3D11Device* device,
      D3D11ShaderCache& cache,
      std::string* outError);
  void ensureD3DShadersImpl(
      ID3D11Device* device,
      D3D11ShaderCache& cache,
      std::string* outError);

private:
  friend class ContextD3D11;
  ShaderStage m_stage = ShaderStage::autoDetect;
  // HLSL source compiled at first pipeline use via ensureD3DShaders.
  std::string m_hlslSource;
  std::string m_hlslEntryPoint;
  Microsoft::WRL::ComPtr<ID3D11VertexShader> m_d3dVertexShader;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> m_d3dPixelShader;
  std::vector<uint8_t> m_bytecode;

  // Guards ensureD3DShaders against concurrent first-call from two
  // pipelines sharing this ShaderModule.
  std::once_flag m_d3dInitFlag;
};
} // namespace rive::ore
