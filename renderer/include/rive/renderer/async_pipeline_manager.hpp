/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/gpu.hpp"
#include "rive/renderer/render_context.hpp"

#include <condition_variable>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <type_traits>

namespace rive::gpu
{

enum class PipelineCreateType
{
    sync,
    async,
};

enum class PipelineStatus
{
    notReady,
    ready,
    errored,
};

// Helper class to manage asynchronous creation of a pipeline (i.e. a
//  combination of a vertex/pixel shader)
template <typename PipelineType> class AsyncPipelineManager
{
public:
    struct PipelineProps
    {
        rive::gpu::DrawType drawType;
        rive::gpu::ShaderFeatures shaderFeatures;
        rive::gpu::InterlockMode interlockMode;
        rive::gpu::ShaderMiscFlags shaderMiscFlags;
#ifdef WITH_RIVE_TOOLS
        rive::gpu::SynthesizedFailureType synthesizedFailureType =
            rive::gpu::SynthesizedFailureType::none;
#endif
    };

    // If PipelineType is movable we'll prefer storing it in an
    //  std::optional instead of a unique_ptr to avoid a heap allocation.
    using PipelineContainerType =
        std::conditional_t<std::is_move_constructible_v<PipelineType> &&
                               std::is_move_assignable_v<PipelineType>,
                           std::optional<PipelineType>,
                           std::unique_ptr<PipelineType>>;

    AsyncPipelineManager(ShaderCompilationMode mode) : m_mode(mode) {}

    AsyncPipelineManager(const AsyncPipelineManager&) = delete;
    AsyncPipelineManager& operator=(const AsyncPipelineManager&) = delete;
    virtual ~AsyncPipelineManager()
    {
        // If we created a background thread, it's important for the derived
        //  class to have already called shutdownBackgroundThread before
        //  destructing its own internal state (otherwise the thread might be
        //  mid-shader-creation when it unloads its context).
        assert(!m_jobThread.joinable());
    }

    const PipelineType* tryGetPipeline(const PipelineProps& propsIn,
                                       const PlatformFeatures& platformFeatures)
    {
        PipelineProps props = propsIn;

        ShaderFeatures ubershaderFeatures =
            gpu::UbershaderFeaturesMaskFor(props.shaderFeatures,
                                           props.drawType,
                                           props.interlockMode,
                                           platformFeatures);

        PipelineCreateType createType;

        switch (m_mode)
        {
            case ShaderCompilationMode::allowAsynchronous:
            default:
                // If this is an ubershader equivalent, we want to create this
                // draw program synchronously so that it is available
                // immediately
                createType = (props.shaderFeatures == ubershaderFeatures)
                                 ? PipelineCreateType::sync
                                 : PipelineCreateType::async;
                break;

            case ShaderCompilationMode::onlyUbershaders:
                // For ubershader-only loading, we'll always use the full
                // ubershader
                //  feature flags and always load synchronously.
                props.shaderFeatures = ubershaderFeatures;
                [[fallthrough]];

            case ShaderCompilationMode::alwaysSynchronous:
                createType = PipelineCreateType::sync;
                break;
        }

        uint32_t key = gpu::ShaderUniqueKey(props.drawType,
                                            props.shaderFeatures,
                                            props.interlockMode,
                                            props.shaderMiscFlags);

        auto iter = m_pipelines.find(key);

#ifdef WITH_RIVE_TOOLS
        // If requested, synthesize a complete failure to get an ubershader
        // (i.e. pretend we attempted to load the current shader asynchronously
        // and tried to fall back on an uber, which failed) (Don't fail on
        // "atomicResolve" because if we fail that one the unit test won't see
        // the clear color)
        if (props.synthesizedFailureType ==
                gpu::SynthesizedFailureType::ubershaderLoad &&
            props.drawType != DrawType::atomicResolve)
        {
            return nullptr;
        }

        if (props.shaderFeatures == ubershaderFeatures)
        {
            // Otherwise, do not synthesize compilation failure for an
            // ubershader.
            props.synthesizedFailureType = gpu::SynthesizedFailureType::none;
        }
#endif

        if (iter == m_pipelines.end())
        {
            // We haven't encountered this shader key yet so it's time to ask
            //  the render context to create the program. If it's happening
            //  asynchronously, it might return us an incomplete pipeline
            //  (for, say, WebGL where there is background shader compilation),
            //  or it might return a nullptr/nullopt value (which means it
            //  queued the work into our jobQueue and we'll get it later)
            auto pipeline = createPipeline(createType, key, props);

            iter = m_pipelines.insert({key, std::move(pipeline)}).first;

            if (createType == PipelineCreateType::sync)
            {
                // If we asked to create a synchronous one, return it
                //  imemdiately (regardless of whether background compilation
                //  has finished or not - the render context is in charge of
                //  stalling on an incomplete shader when it gets one)
                assert(iter->second);

                if (getPipelineStatus(*iter->second) != PipelineStatus::errored)
                {
                    return &*iter->second;
                }

                if (props.shaderFeatures == ubershaderFeatures)
                {
                    // Ubershader creation failed
                    return nullptr;
                }

                // This pipeline failed to build for some reason, but we can
                // (potentially) fall back on the ubershader.
                assert(props.shaderFeatures != ubershaderFeatures);
            }
        }

        if (!iter->second)
        {
            // We don't have this shader yet, so run through the list of
            //  completed shaders and see
            CompletedJob completedJob;
            while (popCompletedJob(&completedJob))
            {
                m_pipelines[completedJob.key] = std::move(completedJob.program);
                if (completedJob.key == key)
                {
                    assert(iter->second);
                    break;
                }
            }
        }

        if (iter->second)
        {
            if (auto status = getPipelineStatus(*iter->second);
                status == PipelineStatus::ready)
            {
                // The program is present and ready to go!
                return &*iter->second;
            }
            else if (status != PipelineStatus::errored)
            {
                assert(status == PipelineStatus::notReady);

                // The program was not ready yet so attempt to move its creation
                //  process forward.
                if (advanceCreation(*iter->second, props))
                {
                    // The program was not previously ready, but it is now.
                    return &*iter->second;
                }
            }
        }

        if (props.shaderFeatures == ubershaderFeatures)
        {
            // The only way to get here for an ubershader should be for it to
            // have failed to compile.
            assert(iter->second &&
                   getPipelineStatus(*iter->second) == PipelineStatus::errored);
            return nullptr;
        }

        // The pipeline is still not ready, so instead return an ubershader
        // version (with all functionality enabled). This will create
        // synchronously so we're guaranteed to have a valid return from this
        // call.
        // NOTE: intentionally not passing along synthesizedFailureType
        //  here because we don't pay attention to it for ubershaders anyway
        return tryGetPipeline({props.drawType,
                               ubershaderFeatures,
                               props.interlockMode,
                               props.shaderMiscFlags},
                              platformFeatures);
    }

protected:
    void clearCacheDoNotCallWithThreadedShaderLoading()
    {
        // If we want to handle this with threaded shaders there's more work
        //  involved, but this was only needed for GL at the moment.
        assert(!m_jobThread.joinable());

        m_pipelines.clear();
    }

    // This function is called to create a pipeline when we don't have one
    //  with its key already. It can be called in either "sync" or "async" mode.
    //  - In async mode, the function should either:
    //   - call queueBackgroundJob with the supplied parameters so that it will
    //     be processed by the background thread (like D3D does)
    //   - create the shader using the target API's built-in async/parallel
    //     thread creation model (like GL does)
    //  - In sync mode, the shader should be created immediately. This mode is
    //    used either when compiling the fallback "ubershader" version of a
    //    drawType (as it is required immediately), or when the background job
    //    thread wants to create a shader (so the implementation of sync mode
    //    needs to be thread-safe when using the target API in that case)
    virtual PipelineContainerType createPipeline(PipelineCreateType,
                                                 uint32_t key,
                                                 const PipelineProps&) = 0;

    // For renderers like GL that have a polling/step-based async setup,
    //  override this function to return whether or not the pipeline is
    //  fully ready or still loading.
    virtual PipelineStatus getPipelineStatus(const PipelineType&) const = 0;

    // For renderers like GL that have a step-based async setup,
    //  override this function to attempt to move the shader along its progress.
    virtual bool advanceCreation(PipelineType&, const PipelineProps&)
    {
        return false; // do nothing by default
    }

    // Called by the render context to use the background threading model to
    //  create pipeline
    void queueBackgroundJob(uint32_t key, const PipelineProps& props)
    {
        // start the job thread if we haven't already
        if (!m_jobThread.joinable())
        {
            m_jobThread =
                std::thread{[this] { backgroundShaderCompilationThread(); }};
        }

        std::unique_lock lock{m_mutex};

        m_jobQueue.push({props, key});
        m_newJobCV.notify_one();
    }

    void shutdownBackgroundThread()
    {
        if (m_jobThread.joinable())
        {
            {
                std::unique_lock lock{m_mutex};
                m_isDone = true;
            }

            m_newJobCV.notify_all();
            m_jobThread.join();
        }
    }

private:
    struct JobParams
    {
        PipelineProps props;
        uint32_t key;
    };

    struct CompletedJob
    {
        uint32_t key;
        PipelineContainerType program;
    };

    bool popCompletedJob(CompletedJob* jobOut)
    {
        std::lock_guard lock{m_mutex};
        if (m_completedJobs.empty())
        {
            return false;
        }

        *jobOut = std::move(m_completedJobs.back());
        m_completedJobs.pop_back();
        return true;
    }

    void backgroundShaderCompilationThread()
    {
        while (true)
        {
            JobParams nextJob;
            {
                std::unique_lock lock{m_mutex};

                while (!m_isDone && m_jobQueue.empty())
                {
                    m_newJobCV.wait(lock);
                }

                if (m_isDone)
                {
                    return;
                }

                nextJob = std::move(m_jobQueue.front());
                m_jobQueue.pop();
            }

            auto newPipeline = createPipeline(PipelineCreateType::sync,
                                              nextJob.key,
                                              nextJob.props);

            {
                std::unique_lock lock{m_mutex};
                m_completedJobs.push_back({
                    nextJob.key,
                    std::move(newPipeline),
                });
            }
        }
    }

    std::map<uint32_t, PipelineContainerType> m_pipelines;

    std::queue<JobParams> m_jobQueue;
    std::vector<CompletedJob> m_completedJobs;

    bool m_isDone = false;
    const ShaderCompilationMode m_mode = ShaderCompilationMode::standard;
    std::thread m_jobThread;
    std::mutex m_mutex;
    std::condition_variable m_newJobCV;
};

} // namespace rive::gpu