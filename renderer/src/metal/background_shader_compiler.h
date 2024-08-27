/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/renderer/gpu.hpp"
#include "rive/renderer/metal/render_context_metal_impl.h"

#include <queue>
#include <thread>

#import <Metal/Metal.h>

namespace rive::gpu
{
// Defines a job to compile a "draw" shader -- either draw_path.glsl or draw_image_mesh.glsl, with a
// specific set of features enabled.
struct BackgroundCompileJob
{
    gpu::DrawType drawType;
    gpu::ShaderFeatures shaderFeatures;
    gpu::InterlockMode interlockMode;
    gpu::ShaderMiscFlags shaderMiscFlags;
    id<MTLLibrary> compiledLibrary;
};

// Compiles "draw" shaders in a background thread. A "draw" shaders is either draw_path.glsl or
// draw_image_mesh.glsl, with a specific set of features enabled.
class BackgroundShaderCompiler
{
public:
    using AtomicBarrierType = PLSRenderContextMetalImpl::AtomicBarrierType;
    using MetalFeatures = PLSRenderContextMetalImpl::MetalFeatures;

    BackgroundShaderCompiler(id<MTLDevice> gpu, MetalFeatures metalFeatures) :
        m_gpu(gpu), m_metalFeatures(metalFeatures)
    {}

    ~BackgroundShaderCompiler();

    void pushJob(const BackgroundCompileJob&);
    bool popFinishedJob(BackgroundCompileJob* job, bool wait);

private:
    void threadMain();

    const id<MTLDevice> m_gpu;
    const MetalFeatures m_metalFeatures;
    std::queue<BackgroundCompileJob> m_pendingJobs;
    std::vector<BackgroundCompileJob> m_finishedJobs;
    std::mutex m_mutex;
    std::condition_variable m_workAddedCondition;
    std::condition_variable m_workFinishedCondition;
    bool m_shouldQuit;
    std::thread m_compilerThread;
};
} // namespace rive::gpu
