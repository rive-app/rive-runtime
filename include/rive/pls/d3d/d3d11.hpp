/*
 * Copyright 2023 Rive
 */

#pragma once

#define NOMINMAX
#include <d3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

#define VERIFY_OK(CODE)                                                                            \
    {                                                                                              \
        HRESULT hr = (CODE);                                                                       \
        if (hr != S_OK)                                                                            \
        {                                                                                          \
            fprintf(stderr, __FILE__ ":%u: D3D error 0x%lx: %s\n", __LINE__, hr, #CODE);           \
            exit(-1);                                                                              \
        }                                                                                          \
    }
