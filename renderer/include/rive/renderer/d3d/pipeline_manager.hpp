/*
 * Copyright 2025 Rive
 */
#include "rive/renderer/async_pipeline_manager.hpp"
#include "rive/renderer/d3d/d3d.hpp"
#include "rive/renderer/gpu.hpp"

#include <string>
#include <unordered_map>

namespace rive::gpu
{
namespace d3d_utils
{
ComPtr<ID3DBlob> compile_shader_to_blob(DrawType drawType,
                                        ShaderFeatures shaderFeatures,
                                        InterlockMode interlockMode,
                                        ShaderMiscFlags shaderMiscFlags,
                                        const D3DCapabilities& d3dCapabilities,
                                        const char* target);
} // namespace d3d_utils

// Handles managing and compiling shaders.
// Has hooks to allow managing pipelines for d3d12
template <typename PipelineType, typename DeviceType>
class D3DPipelineManager : public AsyncPipelineManager<PipelineType>
{
    using Super = AsyncPipelineManager<PipelineType>;

public:
    using VertexShaderType = typename PipelineType::VertexShaderType;
    using FragmentShaderType = typename PipelineType::FragmentShaderType;

    const D3DCapabilities d3dCapabilities() const { return m_d3dCapabilities; }
    DeviceType* device() const { return m_device.Get(); }

    D3DPipelineManager(ComPtr<DeviceType> device,
                       const D3DCapabilities& capabilities,
                       ShaderCompilationMode shaderCompilationMode,
                       const char* vertexTarget,
                       const char* pixelTarget) :
        Super(shaderCompilationMode),
        m_device(device),
        m_d3dCapabilities(capabilities),
        m_vertexTarget(vertexTarget),
        m_pixelTarget(pixelTarget)
    {}
    virtual ~D3DPipelineManager() = default;

protected:
    using PipelineProps = typename Super::PipelineProps;

    virtual std::unique_ptr<FragmentShaderType>
    compilePixelShaderBlobToFinalType(ComPtr<ID3DBlob> blob) = 0;

    virtual std::unique_ptr<VertexShaderType>
    compileVertexShaderBlobToFinalType(DrawType drawType,
                                       ComPtr<ID3DBlob> blob) = 0;

    virtual std::unique_ptr<PipelineType> linkPipeline(
        const PipelineProps& props,
        const VertexShaderType& vs,
        const FragmentShaderType& ps) = 0;

    virtual PipelineStatus getPipelineStatus(
        const PipelineType& pipeline) const override final
    {
        // D3D pipelines never exist in the waiting state, they spring to
        // life completed (or errored)
        return pipeline.succeeded() ? PipelineStatus::ready
                                    : PipelineStatus::errored;
    }

private:
    virtual std::unique_ptr<VertexShaderType> createVertexShader(
        DrawType drawType,
        ShaderFeatures shaderFeatures,
        InterlockMode interlockMode) override
    {
        auto blob = d3d_utils::compile_shader_to_blob(drawType,
                                                      shaderFeatures,
                                                      interlockMode,
                                                      ShaderMiscFlags::none,
                                                      m_d3dCapabilities,
                                                      m_vertexTarget);

        return compileVertexShaderBlobToFinalType(drawType, blob.Get());
    }

    virtual std::unique_ptr<FragmentShaderType> createFragmentShader(
        DrawType drawType,
        ShaderFeatures shaderFeatures,
        InterlockMode interlockMode,
        ShaderMiscFlags miscFlags) override
    {
        auto blob = d3d_utils::compile_shader_to_blob(drawType,
                                                      shaderFeatures,
                                                      interlockMode,
                                                      miscFlags,
                                                      m_d3dCapabilities,
                                                      m_pixelTarget);

        return compilePixelShaderBlobToFinalType(blob.Get());
    }

    virtual std::unique_ptr<PipelineType> createPipeline(
        PipelineCreateType createType,
        uint32_t key,
        const PipelineProps& props) override final
    {
        if (createType == PipelineCreateType::async)
        {
            this->queueBackgroundJob(key, props);
            return nullptr;
        }

        auto& vs = this->getVertexShaderSynchronous(props.drawType,
                                                    props.shaderFeatures,
                                                    props.interlockMode);

        auto& ps = this->getFragmentShaderSynchronous(props.drawType,
                                                      props.shaderFeatures,
                                                      props.interlockMode,
                                                      props.shaderMiscFlags);

        return linkPipeline(props, vs, ps);
    }

    ComPtr<DeviceType> m_device;
    const D3DCapabilities m_d3dCapabilities;

    const char* m_vertexTarget;
    const char* m_pixelTarget;
};

}; // namespace rive::gpu