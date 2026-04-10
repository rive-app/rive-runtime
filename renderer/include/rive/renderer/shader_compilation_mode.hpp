/*
 * Copyright 2026 Rive
 */

#pragma once

namespace rive::gpu
{
enum class ShaderCompilationMode
{
    allowAsynchronous,
    alwaysSynchronous,
    onlyUbershaders,

    // The default mode is to allow asynchronous compilation where available.
    standard = allowAsynchronous,
};
} // namespace rive::gpu
