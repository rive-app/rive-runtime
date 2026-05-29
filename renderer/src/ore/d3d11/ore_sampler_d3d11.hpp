#pragma once
#include "rive/renderer/ore/ore_sampler.hpp"
#include <d3d11.h>
#include <wrl/client.h>

namespace rive::ore
{
class ContextD3D11;

class SamplerD3D11 : public LITE_RTTI_OVERRIDE(Sampler, SamplerD3D11)
{
public:
    SamplerD3D11() = default;
    ~SamplerD3D11() override = default; // ComPtr released automatically

private:
    friend class ContextD3D11;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_d3dSampler;
};
} // namespace rive::ore
