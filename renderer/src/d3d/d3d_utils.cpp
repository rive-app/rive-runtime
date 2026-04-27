/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/d3d/d3d_utils.hpp"

#include <dxgi1_6.h>
#include <iostream>
#include <Windows.h>
#include <winerror.h>
#include "rive/renderer/d3d/d3d.hpp"
namespace rive::gpu
{
namespace d3d_utils
{

DXGI_FORMAT convert_format(GPUTextureFormat format)
{
    switch (format)
    {
        case GPUTextureFormat::rgba32:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case GPUTextureFormat::bc1:
            return DXGI_FORMAT_BC1_UNORM;
        case GPUTextureFormat::bc2:
            return DXGI_FORMAT_BC2_UNORM;
        case GPUTextureFormat::bc3:
            return DXGI_FORMAT_BC3_UNORM;
        case GPUTextureFormat::bc7:
            return DXGI_FORMAT_BC7_UNORM;
        default:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}

bool GetWStringFromString(const char* cString, std::wstring& outString)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, cString, -1, nullptr, 0);
    if (len <= 0)
    {
        std::cerr
            << "Failed to get required size for gpu name filter wide string with error "
            << GetLastError() << "\n";
        return false;
    }

    outString.resize(len - 1, 0);

    if (MultiByteToWideChar(CP_UTF8, 0, cString, -1, &outString[0], len) <= 0)
    {
        std::cerr
            << "Failed to conver gpu name filter to wide string with error "
            << GetLastError() << "\n";
        return false;
    }

    return true;
}
} // namespace d3d_utils
} // namespace rive::gpu