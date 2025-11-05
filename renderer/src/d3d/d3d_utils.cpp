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