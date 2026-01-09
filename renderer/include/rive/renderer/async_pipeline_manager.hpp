/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/gpu.hpp"
#include "rive/renderer/render_context.hpp"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

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

struct StandardPipelineProps
{
    DrawType drawType;
    ShaderFeatures shaderFeatures;
    InterlockMode interlockMode;
    ShaderMiscFlags shaderMiscFlags;
#ifdef WITH_RIVE_TOOLS
    SynthesizedFailureType synthesizedFailureType =
        SynthesizedFailureType::none;
#endif

    uint32_t createKey() const
    {
        return gpu::ShaderUniqueKey(drawType,
                                    shaderFeatures,
                                    interlockMode,
                                    shaderMiscFlags);
    }
};

// Helper class to manage asynchronous creation of a pipeline (i.e. a
//  combination of a vertex/pixel shader)
template <typename PipelineType> class AsyncPipelineManager
{
public:
    using PipelineProps = typename PipelineType::PipelineProps;
    using VertexShaderType = typename PipelineType::VertexShaderType;
    using FragmentShaderType = typename PipelineType::FragmentShaderType;

    // The pipeline key might be 32- or 64-bit depending on renderer, so detect
    // which it is.
    using PipelineKey = decltype(std::declval<PipelineProps>().createKey());

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

        PipelineKey key = props.createKey();

        auto iter = m_pipelines.find(key);

#ifdef WITH_RIVE_TOOLS
        // If requested, synthesize a complete failure to get an ubershader
        // (i.e. pretend we attempted to load the current shader asynchronously
        // and tried to fall back on an uber, which failed) (Don't fail on
        // "renderPassResolve" in atomic mode because if we fail that one the
        // unit test won't see the clear color.)
        if (props.synthesizedFailureType ==
                gpu::SynthesizedFailureType::ubershaderLoad &&
            props.drawType != DrawType::renderPassResolve)
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
#ifdef WITH_RIVE_TOOLS
                    assert(props.synthesizedFailureType !=
                           SynthesizedFailureType::none);
#else
                    assert(false && "Ubershader creation failed");
#endif
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
        assert(props.shaderFeatures != ubershaderFeatures);
        auto ubershaderProps = props;
        ubershaderProps.shaderFeatures = ubershaderFeatures;
        return tryGetPipeline(ubershaderProps, platformFeatures);
    }

    const VertexShaderType& getVertexShaderSynchronous(
        DrawType drawType,
        ShaderFeatures shaderFeatures,
        InterlockMode interlockMode)
    {
        // Remove any non-vertex-shader features before doing the key
        shaderFeatures &= kVertexShaderFeaturesMask;

        return getSharedObjectSynchronous(
            gpu::ShaderUniqueKey(drawType,
                                 shaderFeatures,
                                 interlockMode,
                                 ShaderMiscFlags::none),
            m_vertexShaderMap,
            [&]() {
                return createVertexShader(drawType,
                                          shaderFeatures,
                                          interlockMode);
            });
    }

    const FragmentShaderType& getFragmentShaderSynchronous(
        DrawType drawType,
        ShaderFeatures shaderFeatures,
        InterlockMode interlockMode,
        ShaderMiscFlags miscFlags)
    {
        return getSharedObjectSynchronous(gpu::ShaderUniqueKey(drawType,
                                                               shaderFeatures,
                                                               interlockMode,
                                                               miscFlags),
                                          m_fragmentShaderMap,
                                          [&]() {
                                              return createFragmentShader(
                                                  drawType,
                                                  shaderFeatures,
                                                  interlockMode,
                                                  miscFlags);
                                          });
    }

    void clearCache()
    {
        std::unique_lock lock{m_mutex};

        // Start by clearing the job queue (There's no reset or clear on
        // std::queue so we have to pop manually). Doing it this way instead of
        // doing "m_jobQueue = {}" to keep the internal buffer intact and avoid
        // some heap allocations the next time we start queueing jobs again
        while (!m_jobQueue.empty())
        {
            m_jobQueue.pop();
        }

        // Now wait for the background thread(s) to finish any work they are
        // actively doing.
        while (m_activePipelineCreationCount > 0)
        {
            m_jobCompleteCV.wait(lock);
        }

        // Clear all of the rest of our cached everything - we'll start over.
        m_completedJobs.clear();
        m_vertexShaderMap.clear();
        m_fragmentShaderMap.clear();
        m_pipelines.clear();
        clearCacheInternal();
    }

protected:
    virtual std::unique_ptr<VertexShaderType> createVertexShader(
        DrawType,
        ShaderFeatures,
        InterlockMode) = 0;

    virtual std::unique_ptr<FragmentShaderType> createFragmentShader(
        DrawType,
        ShaderFeatures,
        InterlockMode,
        ShaderMiscFlags) = 0;

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
    virtual std::unique_ptr<PipelineType> createPipeline(
        PipelineCreateType,
        PipelineKey key,
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

    // If renderers have extra state, this is where they can clear it while
    // within the safety of the mutex lock
    virtual void clearCacheInternal() {}

    // Called by the render context to use the background threading model to
    //  create pipeline
    void queueBackgroundJob(PipelineKey key, const PipelineProps& props)
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

    template <typename KeyType, typename SharedObject, typename CreateFunc>
    SharedObject& getSharedObjectSynchronous(
        KeyType key,
        std::unordered_map<uint32_t, std::unique_ptr<SharedObject>>& map,
        CreateFunc&& createFunc)
    {
        // See if the object exists already first
        {
            std::unique_lock lock{m_mutex};

            auto iter = map.find(key);
            if (iter != map.end())
            {
                while (iter->second == nullptr)
                {
                    // This is either a synchronous object request and an
                    // asynchronous build is running it *or* it's on an async
                    // thread and another thread is running it - either way we
                    // need to wait for the thread making this object to finish
                    // (so we only build it once)
                    m_sharedObjectReadyCV.wait(lock);
                }

                return *iter->second;
            }

            // Insert an empty entry into the object map so the other threads
            // don't try to double-up on creation of this object
            map.try_emplace(key);
        }

        // Now that we're outside of the lock, ask the renderer context to
        // create the object
        auto object = createFunc();

        {
            std::unique_lock lock{m_mutex};

            auto iter = map.find(key);
            assert(iter != map.end());
            assert(iter->second == nullptr);
            iter->second = std::move(object);
            m_sharedObjectReadyCV.notify_all();
            return *iter->second;
        }
    }

private:
    struct JobParams
    {
        PipelineProps props;
        PipelineKey key;
    };

    struct CompletedJob
    {
        PipelineKey key;
        std::unique_ptr<PipelineType> program;
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
                m_activePipelineCreationCount++;
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
                m_activePipelineCreationCount--;
                m_jobCompleteCV.notify_all();
            }
        }
    }

    std::unordered_map<uint32_t, std::unique_ptr<VertexShaderType>>
        m_vertexShaderMap;
    std::unordered_map<uint32_t, std::unique_ptr<FragmentShaderType>>
        m_fragmentShaderMap;
    std::unordered_map<PipelineKey, std::unique_ptr<PipelineType>> m_pipelines;

    std::queue<JobParams> m_jobQueue;
    std::vector<CompletedJob> m_completedJobs;

    bool m_isDone = false;
    uint32_t m_activePipelineCreationCount = 0;
    const ShaderCompilationMode m_mode = ShaderCompilationMode::standard;
    std::thread m_jobThread;
    std::mutex m_mutex;
    std::condition_variable m_newJobCV;
    std::condition_variable m_jobCompleteCV;
    std::condition_variable m_sharedObjectReadyCV;
};

} // namespace rive::gpu
