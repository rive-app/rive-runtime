/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/pls/pls.hpp"
#include "rive/pls/metal/pls_render_context_metal_impl.h"

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
    pls::InterlockMode interlockMode;
    pls::ShaderMiscFlags shaderMiscFlags;
    id<MTLLibrary> compiledLibrary;
};

// Compiles "draw" shaders in a background thread. A "draw" shaders is either draw_path.glsl or
// draw_image_mesh.glsl, with a specific set of features enabled.
class BackgroundShaderCompiler
{
public:
    using AtomicBarrierType = PLSRenderContextMetalImpl::AtomicBarrierType;

    BackgroundShaderCompiler(id<MTLDevice> gpu, AtomicBarrierType atomicBarrierType) :
        m_gpu(gpu), m_atomicBarrierType(atomicBarrierType)
    {}

    ~BackgroundShaderCompiler();

    void pushJob(const BackgroundCompileJob&);
    bool popFinishedJob(BackgroundCompileJob* job, bool wait);

private:
    void threadMain();

    const id<MTLDevice> m_gpu;
    const AtomicBarrierType m_atomicBarrierType;
    std::queue<BackgroundCompileJob> m_pendingJobs;
    std::vector<BackgroundCompileJob> m_finishedJobs;
    std::mutex m_mutex;
    std::condition_variable m_workAddedCondition;
    std::condition_variable m_workFinishedCondition;
    bool m_shouldQuit = false;
    std::thread m_compilerThread;
};
} // namespace rive::pls
