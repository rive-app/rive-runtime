/*
 * Copyright 2025 Rive
 */

#pragma once

#include <d3dcommon.h>
#include <wrl/client.h>
#include <system_error>

#include "rive/renderer/render_context.hpp"

using Microsoft::WRL::ComPtr;

#define VERIFY_OK(CODE)                                                        \
    {                                                                          \
        HRESULT hr = (CODE);                                                   \
        if (hr != S_OK)                                                        \
        {                                                                      \
            fprintf(stderr,                                                    \
                    __FILE__ ":%i: D3D error %s: %s\n",                        \
                    static_cast<int>(__LINE__),                                \
                    std::system_category().message(hr).c_str(),                \
                    #CODE);                                                    \
            abort();                                                           \
        }                                                                      \
    }
namespace rive::gpu
{
struct D3DCapabilities
{
    bool supportsRasterizerOrderedViews = false;
    bool supportsTypedUAVLoadStore =
        false; // Can we load/store all UAV formats used by Rive?
    bool supportsMin16Precision =
        false; // Can we use minimum 16-bit types (e.g. min16int)?
    bool isIntel = false;
    bool allowsUAVSlot0WithColorOutput = true;
};

struct D3DContextOptions
{
    ShaderCompilationMode shaderCompilationMode =
        ShaderCompilationMode::standard;
    bool disableRasterizerOrderedViews = false; // Primarily for testing.
    bool disableTypedUAVLoadStore = false;      // Primarily for testing.
    bool isIntel = false;
};
} // namespace rive::gpu
