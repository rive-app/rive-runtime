/*
 * Copyright 2023 Rive
 */

#pragma once

#include <d3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

#define VERIFY_OK(CODE)                                                                            \
    {                                                                                              \
        HRESULT hr = (CODE);                                                                       \
        if (hr != S_OK)                                                                            \
        {                                                                                          \
            fprintf(stderr,                                                                        \
                    __FILE__ ":%i: D3D error 0x%lx: %s\n",                                         \
                    static_cast<int>(__LINE__),                                                    \
                    hr,                                                                            \
                    #CODE);                                                                        \
            abort();                                                                               \
        }                                                                                          \
    }
