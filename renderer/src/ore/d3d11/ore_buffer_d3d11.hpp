#pragma once
#include "rive/renderer/ore/ore_buffer.hpp"
#include <d3d11.h>
#include <wrl/client.h>

namespace rive::ore
{
class ContextD3D11;

class BufferD3D11 : public LITE_RTTI_OVERRIDE(Buffer, BufferD3D11)
{
public:
    BufferD3D11(uint32_t size, BufferUsage usage) :
        lite_rtti_override(size, usage)
    {}
    ~BufferD3D11() override = default; // ComPtr released automatically
    void update(const void* data, uint32_t size, uint32_t offset) override;

private:
    friend class ContextD3D11;
    friend class RenderPassD3D11;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_d3d11Buffer;
    ID3D11DeviceContext* m_d3d11Context = nullptr; // Weak ref.
};
} // namespace rive::ore
