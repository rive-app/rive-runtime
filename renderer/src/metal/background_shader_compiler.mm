/*
 * Copyright 2023 Rive
 */

#include "background_shader_compiler.h"

#include "generated/shaders/metal.glsl.hpp"
#include "generated/shaders/constants.glsl.hpp"
#include "generated/shaders/common.glsl.hpp"
#include "generated/shaders/advanced_blend.glsl.hpp"
#include "generated/shaders/draw_path_common.glsl.hpp"
#include "generated/shaders/draw_path.glsl.hpp"
#include "generated/shaders/draw_image_mesh.glsl.hpp"

#ifndef RIVE_IOS
// iOS doesn't need the atomic shaders; every non-simulated iOS device supports
// framebuffer reads.
#include "generated/shaders/atomic_draw.glsl.hpp"
#endif

#include <sstream>

namespace rive::gpu
{
BackgroundShaderCompiler::~BackgroundShaderCompiler()
{
    if (m_compilerThread.joinable())
    {
        {
            std::lock_guard lock(m_mutex);
            m_shouldQuit = true;
        }

        m_workAddedCondition.notify_all();
        m_compilerThread.join();
    }
}

void BackgroundShaderCompiler::pushJob(const BackgroundCompileJob& job)
{
    {
        std::lock_guard lock(m_mutex);
        if (!m_compilerThread.joinable())
        {
            m_compilerThread =
                std::thread(&BackgroundShaderCompiler::threadMain, this);
        }
        m_pendingJobs.push(std::move(job));
    }
    m_workAddedCondition.notify_all();
}

bool BackgroundShaderCompiler::popFinishedJob(BackgroundCompileJob* job,
                                              bool wait)
{
    std::unique_lock lock(m_mutex);
    while (m_finishedJobs.empty())
    {
        if (!wait)
        {
            return false;
        }
        m_workFinishedCondition.wait(lock);
    }
    *job = std::move(m_finishedJobs.back());
    m_finishedJobs.pop_back();
    return true;
}

void BackgroundShaderCompiler::threadMain()
{
    BackgroundCompileJob job;
    std::unique_lock lock(m_mutex);
    for (;;)
    {
        while (m_pendingJobs.empty() && !m_shouldQuit)
        {
            m_workAddedCondition.wait(lock);
        }

        if (m_shouldQuit)
        {
            return;
        }

        job = std::move(m_pendingJobs.front());
        m_pendingJobs.pop();

        lock.unlock();

        gpu::DrawType drawType = job.drawType;
        gpu::ShaderFeatures shaderFeatures = job.shaderFeatures;
        gpu::InterlockMode interlockMode = job.interlockMode;
        gpu::ShaderMiscFlags shaderMiscFlags = job.shaderMiscFlags;

        auto defines = [[NSMutableDictionary alloc] init];
        defines[@GLSL_VERTEX] = @"";
        defines[@GLSL_FRAGMENT] = @"";
        for (size_t i = 0; i < gpu::kShaderFeatureCount; ++i)
        {
            ShaderFeatures feature = static_cast<ShaderFeatures>(1 << i);
            if (shaderFeatures & feature)
            {
                const char* macro = gpu::GetShaderFeatureGLSLName(feature);
                defines[[NSString stringWithUTF8String:macro]] = @"1";
            }
        }
        if (interlockMode == gpu::InterlockMode::atomics)
        {
            // Atomic mode uses device buffers instead of framebuffer fetches.
            defines[@GLSL_PLS_IMPL_DEVICE_BUFFER] = @"";
            if (m_metalFeatures.atomicBarrierType ==
                AtomicBarrierType::rasterOrderGroup)
            {
                defines[@GLSL_PLS_IMPL_DEVICE_BUFFER_RASTER_ORDERED] = @"";
            }
            if (shaderMiscFlags &
                gpu::ShaderMiscFlags::fixedFunctionColorOutput)
            {
                defines[@GLSL_FIXED_FUNCTION_COLOR_OUTPUT] = @"";
            }
        }
        if (shaderMiscFlags & gpu::ShaderMiscFlags::clockwiseFill)
        {
            defines[@GLSL_CLOCKWISE_FILL] = @"1";
        }

        auto source =
            [[NSMutableString alloc] initWithCString:gpu::glsl::metal
                                            encoding:NSUTF8StringEncoding];
        [source
            appendFormat:@"%s\n%s\n", gpu::glsl::constants, gpu::glsl::common];
        if (shaderFeatures & ShaderFeatures::ENABLE_ADVANCED_BLEND)
        {
            [source appendFormat:@"%s\n", gpu::glsl::advanced_blend];
        }

        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::midpointFanCenterAAPatches:
            case DrawType::outerCurvePatches:
                // Add baseInstance to the instanceID for path draws.
                defines[@GLSL_ENABLE_INSTANCE_INDEX] = @"";
                defines[@GLSL_DRAW_PATH] = @"";
                [source appendFormat:@"%s\n", gpu::glsl::draw_path_common];
#ifdef RIVE_IOS
                [source appendFormat:@"%s\n", gpu::glsl::draw_path];
#else
                [source appendFormat:@"%s\n",
                                     interlockMode ==
                                             gpu::InterlockMode::rasterOrdering
                                         ? gpu::glsl::draw_path
                                         : gpu::glsl::atomic_draw];
#endif
                break;
            case DrawType::atlasBlit:
                defines[@GLSL_ATLAS_BLIT] = @"1";
                [[fallthrough]];
            case DrawType::interiorTriangulation:
                defines[@GLSL_DRAW_INTERIOR_TRIANGLES] = @"";
                [source appendFormat:@"%s\n", gpu::glsl::draw_path_common];
#ifdef RIVE_IOS
                [source appendFormat:@"%s\n", gpu::glsl::draw_path];
#else
                [source appendFormat:@"%s\n",
                                     interlockMode ==
                                             gpu::InterlockMode::rasterOrdering
                                         ? gpu::glsl::draw_path
                                         : gpu::glsl::atomic_draw];
#endif
                break;
            case DrawType::imageRect:
#ifdef RIVE_IOS
                RIVE_UNREACHABLE();
#else
                assert(interlockMode == InterlockMode::atomics);
                defines[@GLSL_DRAW_IMAGE] = @"";
                defines[@GLSL_DRAW_IMAGE_RECT] = @"";
                [source appendFormat:@"%s\n", gpu::glsl::draw_path_common];
                [source appendFormat:@"%s\n", gpu::glsl::atomic_draw];
#endif
                break;
            case DrawType::imageMesh:
                defines[@GLSL_DRAW_IMAGE] = @"";
                defines[@GLSL_DRAW_IMAGE_MESH] = @"";
#ifdef RIVE_IOS
                [source appendFormat:@"%s\n", gpu::glsl::draw_image_mesh];
#else
                if (interlockMode == gpu::InterlockMode::rasterOrdering)
                {
                    [source appendFormat:@"%s\n", gpu::glsl::draw_image_mesh];
                }
                else
                {
                    [source appendFormat:@"%s\n", gpu::glsl::draw_path_common];
                    [source appendFormat:@"%s\n", gpu::glsl::atomic_draw];
                }
#endif
                break;
            case DrawType::atomicInitialize:
#ifdef RIVE_IOS
                RIVE_UNREACHABLE();
#else
                assert(interlockMode == InterlockMode::atomics);
                defines[@GLSL_DRAW_RENDER_TARGET_UPDATE_BOUNDS] = @"";
                defines[@GLSL_INITIALIZE_PLS] = @"";
                if (shaderMiscFlags & gpu::ShaderMiscFlags::storeColorClear)
                {
                    defines[@GLSL_STORE_COLOR_CLEAR] = @"";
                }
                if (shaderMiscFlags &
                    gpu::ShaderMiscFlags::swizzleColorBGRAToRGBA)
                {
                    defines[@GLSL_SWIZZLE_COLOR_BGRA_TO_RGBA] = @"";
                }
                [source appendFormat:@"%s\n", gpu::glsl::draw_path_common];
                [source appendFormat:@"%s\n", gpu::glsl::atomic_draw];
#endif
                break;
            case DrawType::atomicResolve:
#ifdef RIVE_IOS
                RIVE_UNREACHABLE();
#else
                assert(interlockMode == InterlockMode::atomics);
                defines[@GLSL_DRAW_RENDER_TARGET_UPDATE_BOUNDS] = @"";
                defines[@GLSL_RESOLVE_PLS] = @"";
                if (shaderMiscFlags &
                    gpu::ShaderMiscFlags::coalescedResolveAndTransfer)
                {
                    defines[@GLSL_COALESCED_PLS_RESOLVE_AND_TRANSFER] = @"";
                }
                [source appendFormat:@"%s\n", gpu::glsl::draw_path_common];
                [source appendFormat:@"%s\n", gpu::glsl::atomic_draw];
#endif
                break;
            case DrawType::msaaStrokes:
            case DrawType::msaaMidpointFanBorrowedCoverage:
            case DrawType::msaaMidpointFans:
            case DrawType::msaaMidpointFanStencilReset:
            case DrawType::msaaMidpointFanPathsStencil:
            case DrawType::msaaMidpointFanPathsCover:
            case DrawType::msaaOuterCubics:
            case DrawType::msaaStencilClipReset:
                RIVE_UNREACHABLE();
        }

        NSError* err = [NSError errorWithDomain:@"compile"
                                           code:200
                                       userInfo:nil];
        MTLCompileOptions* compileOptions = [MTLCompileOptions new];
#if defined(RIVE_IOS) || defined(RIVE_IOS_SIMULATOR)
        compileOptions.languageVersion =
            MTLLanguageVersion2_2; // On ios, we need version 2.2+
#else
        compileOptions.languageVersion =
            MTLLanguageVersion2_3; // On mac, we need version 2.3+
#endif
        compileOptions.fastMathEnabled = YES;
        if (@available(iOS 14, *))
        {
            compileOptions.preserveInvariance = YES;
        }
        compileOptions.preprocessorMacros = defines;
#ifdef WITH_RIVE_TOOLS
        if (job.synthesizedFailureType ==
            SynthesizedFailureType::shaderCompilation)
        {
            assert(job.compiledLibrary == nil);
        }
        else
#endif
        {
            job.compiledLibrary = [m_gpu newLibraryWithSource:source
                                                      options:compileOptions
                                                        error:&err];
        }

        lock.lock();

        if (job.compiledLibrary == nil)
        {
#ifdef WITH_RIVE_TOOLS
            if (job.synthesizedFailureType ==
                SynthesizedFailureType::shaderCompilation)
            {
                NSLog(@"Synthesizing shader compilation failure...");
            }
            else
#endif
            {
                // The compile job failed, most likely to external environmental
                // factors. Give up on this shader and let the render context
                // fall back on an uber shader instead.
                int lineNumber = 1;
                std::stringstream stream(source.UTF8String);
                std::string lineStr;
                while (std::getline(stream, lineStr, '\n'))
                {
                    NSLog(@"%4i| %s", lineNumber++, lineStr.c_str());
                }
                NSLog(@"%@", err.localizedDescription);
            }

            NSLog(@"Failed to compile shader.");
            assert(false
#ifdef WITH_RIVE_TOOLS
                   || job.synthesizedFailureType ==
                          SynthesizedFailureType::shaderCompilation
#endif
            );
        }

        m_finishedJobs.push_back(std::move(job));
        m_workFinishedCondition.notify_all();
    }
}
} // namespace rive::gpu
