/*
 * Copyright 2023 Rive
 */

#include "background_shader_compiler.h"

#include "../out/obj/generated/metal.glsl.hpp"
#include "../out/obj/generated/constants.glsl.hpp"
#include "../out/obj/generated/common.glsl.hpp"
#include "../out/obj/generated/advanced_blend.glsl.hpp"
#include "../out/obj/generated/draw_path.glsl.hpp"
#include "../out/obj/generated/draw_image_mesh.glsl.hpp"

namespace rive::pls
{
BackgroundShaderCompiler::~BackgroundShaderCompiler()
{
    if (m_compilerThread.joinable())
    {
        m_shouldQuit = true;
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
            m_compilerThread = std::thread(&BackgroundShaderCompiler::threadMain, this);
        }
        m_pendingJobs.push(std::move(job));
    }
    m_workAddedCondition.notify_all();
}

bool BackgroundShaderCompiler::popFinishedJob(BackgroundCompileJob* job, bool wait)
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

        pls::DrawType drawType = job.drawType;
        pls::ShaderFeatures shaderFeatures = job.shaderFeatures;

        auto defines = [[NSMutableDictionary alloc] init];
        defines[@GLSL_VERTEX] = @"";
        defines[@GLSL_FRAGMENT] = @"";
        for (size_t i = 0; i < pls::kShaderFeatureCount; ++i)
        {
            ShaderFeatures feature = static_cast<ShaderFeatures>(1 << i);
            if (shaderFeatures & feature)
            {
                const char* macro = pls::GetShaderFeatureGLSLName(feature);
                defines[[NSString stringWithUTF8String:macro]] = @"";
            }
        }

        auto source = [[NSMutableString alloc] initWithCString:pls::glsl::metal
                                                      encoding:NSUTF8StringEncoding];
        [source appendFormat:@"%s\n%s\n", pls::glsl::constants, pls::glsl::common];
        if (shaderFeatures & ShaderFeatures::ENABLE_ADVANCED_BLEND)
        {
            [source appendFormat:@"%s\n", pls::glsl::advanced_blend];
        }

        switch (drawType)
        {
            case DrawType::interiorTriangulation:
                defines[@GLSL_DRAW_INTERIOR_TRIANGLES] = @"";
                [[fallthrough]];
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
                [source appendFormat:@"%s\n", pls::glsl::draw_path];
                break;
            case DrawType::imageMesh:
                [source appendFormat:@"%s\n", pls::glsl::draw_image_mesh];
                break;
        }

        NSError* err = [NSError errorWithDomain:@"pls_compile" code:200 userInfo:nil];
        MTLCompileOptions* compileOptions = [MTLCompileOptions new];
        compileOptions.fastMathEnabled = YES;
        if (@available(iOS 14, *))
        {
            compileOptions.preserveInvariance = YES;
        }
        compileOptions.preprocessorMacros = defines;
        job.compiledLibrary = [m_gpu newLibraryWithSource:source options:compileOptions error:&err];
        if (job.compiledLibrary == nil)
        {
            fprintf(stderr, "Failed to compile shader.\n\n");
            fprintf(stderr, "%s\n", err.localizedDescription.UTF8String);
            exit(-1);
        }

        lock.lock();

        m_finishedJobs.push_back(std::move(job));
        m_workFinishedCondition.notify_all();
    }
}
} // namespace rive::pls
