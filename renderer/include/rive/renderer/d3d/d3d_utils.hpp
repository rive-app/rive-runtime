/*
 * Copyright 2025 Rive
 */
#include <string>
#include "rive/renderer/render_context.hpp"
#include "rive/texture_archive.hpp"
#include <dxgiformat.h>

namespace rive::gpu
{
namespace d3d_utils
{
bool GetWStringFromString(const char*, std::wstring& outString);

DXGI_FORMAT convert_format(GPUTextureFormat format);

}; // namespace d3d_utils
}; // namespace rive::gpu
