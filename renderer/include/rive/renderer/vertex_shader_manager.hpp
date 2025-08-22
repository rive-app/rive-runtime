#include "rive/renderer/gpu.hpp"

#include <condition_variable>
#include <map>
#include <mutex>
#include <optional>

namespace rive::gpu
{

// Helper class to wrap the handling of sharing vertex shaders between multiple
//  pipeline states (since vertex shaders only use a subset of the total draw
//  flags)
template <typename VSStorageType> class VertexShaderManager
{
public:
    VertexShaderManager() = default;
    VertexShaderManager(const VertexShaderManager&) = delete;
    VertexShaderManager& operator=(const VertexShaderManager&) = delete;

    const VSStorageType& getShader(rive::gpu::DrawType drawType,
                                   rive::gpu::ShaderFeatures shaderFeatures,
                                   rive::gpu::InterlockMode interlockMode)
    {
        // Filter out the things that don't matter for vertex shaders
        shaderFeatures &= kVertexShaderFeaturesMask;

        uint32_t key = gpu::ShaderUniqueKey(drawType,
                                            shaderFeatures,
                                            interlockMode,
                                            gpu::ShaderMiscFlags::none);

        {
            std::unique_lock lock{m_mutex};

            auto iter = m_shaders.find(key);
            if (iter != m_shaders.end())
            {
                while (!iter->second.has_value())
                {
                    // This is either a synchronous shader request and an
                    //  asynchronous build is running it *or* it's on an async
                    //  thread and another thread is running it - either way
                    //  we need to wait for the thread making this shader to
                    //  finish (so we only build it once)
                    m_vsCompleteCV.wait(lock);
                }

                return iter->second.value();
            }

            // Insert an empty entry into the shader map so that other
            //  threads don't try to double-up on creation of this shader.
            m_shaders.try_emplace(key);
        }

        // Now we're outside of the lock, ask the renderer context to create the
        //  shader
        auto shaderContainer =
            createVertexShader(drawType, shaderFeatures, interlockMode);

        {
            std::unique_lock lock{m_mutex};

            auto iter = m_shaders.find(key);
            assert(iter != m_shaders.end());
            assert(!iter->second.has_value());
            iter->second.emplace(std::move(shaderContainer));

            m_vsCompleteCV.notify_all();

            return iter->second.value();
        }
    }

protected:
    virtual VSStorageType createVertexShader(
        gpu::DrawType drawType,
        gpu::ShaderFeatures shaderFeatures,
        gpu::InterlockMode interlockMode) = 0;

private:
    std::map<uint32_t, std::optional<VSStorageType>> m_shaders;

    std::mutex m_mutex;
    std::condition_variable m_vsCompleteCV;
};

} // namespace rive::gpu