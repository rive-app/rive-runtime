/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/pls/pls.hpp"

#include <queue>
#include <thread>

#import <Metal/Metal.h>

namespace rive::pls
{
// Defines a job to compile a "draw" shader -- either draw_path.glsl or draw_image_mesh.glsl, with a
// specific set of features enabled.
struct BackgroundCompileJob
{
    pls::DrawType drawType;
    pls::ShaderFeatures shaderFeatures;
    id<MTLLibrary> compiledLibrary;
};

// Compiles "draw" shaders in a background thread. A "draw" shaders is either draw_path.glsl or
// draw_image_mesh.glsl, with a specific set of features enabled.
class BackgroundShaderCompiler
{
public:
    BackgroundShaderCompiler(id<MTLDevice> gpu) : m_gpu(gpu) {}
    ~BackgroundShaderCompiler();

    void pushJob(const BackgroundCompileJob&);
    bool popFinishedJob(BackgroundCompileJob* job, bool wait);

private:
    void threadMain();

    const id<MTLDevice> m_gpu;
    std::queue<BackgroundCompileJob> m_pendingJobs;
    std::vector<BackgroundCompileJob> m_finishedJobs;
    std::mutex m_mutex;
    std::condition_variable m_workAddedCondition;
    std::condition_variable m_workFinishedCondition;
    bool m_shouldQuit;
    std::thread m_compilerThread;
};
} // namespace rive::pls
