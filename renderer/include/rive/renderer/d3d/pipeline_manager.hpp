/*
 * Copyright 2025 Rive
 */
#include "rive/renderer/d3d/d3d.hpp"
#include "rive/renderer/gpu.hpp"

#include <string>
#include <unordered_map>

namespace rive::gpu
{
namespace d3d_utils
{
// generates a shader using ostringstream
std::string build_shader(DrawType drawType,
                         ShaderFeatures shaderFeatures,
                         InterlockMode interlockMode,
                         ShaderMiscFlags shaderMiscFlags,
                         const D3DCapabilities& d3dCapabilities);
// compile shader to source using "main" as an entry point
ComPtr<ID3DBlob> compile_vertex_source_to_blob(const std::string& source,
                                               const char* target);
// compile shader to source using "main" as an entry point
ComPtr<ID3DBlob> compile_pixel_source_to_blob(const std::string& source,
                                              const char* target);
} // namespace d3d_utils

// Handles managing and compiling shaders.
// Has hooks to allow managing pipelines for d3d12
// Will also manages threading for just in time compiling of shaders once
// thats in
template <typename VetexShaderType,
          typename PixelShaderType,
          typename DeviceType>
class D3DPipelineManager
{
public:
    // everything needed to compile a vertex + pixel shader combo
    struct ShaderCompileRequest
    {
        DrawType drawType;
        ShaderFeatures shaderFeatures;
        InterlockMode interlockMode;
        ShaderMiscFlags shaderMiscFlags;
        const D3DCapabilities& d3dCapabilities;
    };

    // shader compiler result including keys for m_drawVertexShaders and
    // m_drawPixelShaders
    struct ShaderCompileResult
    {
        uint32_t vertexShaderKey;
        uint32_t pixelShaderKey;
        struct VertexResult
        {
            bool hasResult = false;
            VetexShaderType vertexShaderResult;
        } vertexResult;
        struct PixelResult
        {
            bool hasResult = false;
            PixelShaderType pixelShaderResult;
        } pixelResult;
        // needed for d3d12
        void* resultData = nullptr;
    };

    D3DPipelineManager(ComPtr<DeviceType> device,
                       const D3DCapabilities& capabilities,
                       const char* vertexTarget,
                       const char* pixelTarget) :
        m_device(std::move(device)),
        m_d3dCapabilities(capabilities),
        m_vertexTarget(vertexTarget),
        m_pixelTarget(pixelTarget)
    {}

    const D3DCapabilities d3dCapabilities() const { return m_d3dCapabilities; }

    DeviceType* device() const { return m_device.Get(); }

protected:
    // called when shaderCompileWorker finished compiling the source shader to
    // blobs and now we need to convert that to the platform specific shader
    // tpes i.e. ID3D11Vertex/PixelShader for 11 and ID3D12PilineState for 12.
    // note: this could be called on a background thread
    virtual void compileBlobToFinalType(const ShaderCompileRequest&,
                                        ComPtr<ID3DBlob> vertexShader,
                                        ComPtr<ID3DBlob> pixelShader,
                                        ShaderCompileResult*) = 0;

    // get shaders with given compiler request. returns true and sets
    // outShaderResult on succes, returns false and outShaderResult is undefined
    // on failure
    bool getShader(const ShaderCompileRequest& shaderCompileRequest,
                   ShaderCompileResult* outShaderResult)
    {
        // dont pass nullptr here
        assert(outShaderResult);

        outShaderResult->vertexShaderKey = gpu::ShaderUniqueKey(
            shaderCompileRequest.drawType,
            shaderCompileRequest.shaderFeatures & kVertexShaderFeaturesMask,
            shaderCompileRequest.interlockMode,
            gpu::ShaderMiscFlags::none);

        outShaderResult->pixelShaderKey =
            ShaderUniqueKey(shaderCompileRequest.drawType,
                            shaderCompileRequest.shaderFeatures,
                            shaderCompileRequest.interlockMode,
                            shaderCompileRequest.shaderMiscFlags);

        auto vertexEntry =
            m_drawVertexShaders.find(outShaderResult->vertexShaderKey);

        auto pixelEntry =
            m_drawPixelShaders.find(outShaderResult->pixelShaderKey);

        if (vertexEntry != m_drawVertexShaders.end())
        {
            outShaderResult->vertexResult.hasResult = true;
            outShaderResult->vertexResult.vertexShaderResult =
                vertexEntry->second;
        }

        if (pixelEntry != m_drawPixelShaders.end())
        {
            outShaderResult->pixelResult.hasResult = true;
            outShaderResult->pixelResult.pixelShaderResult = pixelEntry->second;
        }

        if (vertexEntry == m_drawVertexShaders.end() ||
            pixelEntry == m_drawPixelShaders.end())
        {
            if (!shaderCompileWorker(shaderCompileRequest, outShaderResult))
            {
                // eventually this means background shader compile happening, so
                // we would set uber shader here and return
                RIVE_UNREACHABLE();
            }
            if (vertexEntry == m_drawVertexShaders.end())
            {
                assert(outShaderResult->vertexResult.hasResult);
                m_drawVertexShaders.insert(
                    {outShaderResult->vertexShaderKey,
                     outShaderResult->vertexResult.vertexShaderResult});
            }

            if (pixelEntry == m_drawPixelShaders.end())
            {
                assert(outShaderResult->pixelResult.hasResult);
                m_drawPixelShaders.insert(
                    {outShaderResult->pixelShaderKey,
                     outShaderResult->pixelResult.pixelShaderResult});
            }
        }

        return true;
    }

private:
    // build shader (currnetly runs synchronously but will be async eventually)
    bool shaderCompileWorker(const ShaderCompileRequest& compileRequest,
                             ShaderCompileResult* outShaderResult)
    {
        assert(outShaderResult->vertexResult.hasResult == false ||
               outShaderResult->pixelResult.hasResult == false);

        auto shader = d3d_utils::build_shader(compileRequest.drawType,
                                              compileRequest.shaderFeatures,
                                              compileRequest.interlockMode,
                                              compileRequest.shaderMiscFlags,
                                              compileRequest.d3dCapabilities);

        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

        if (!outShaderResult->vertexResult.hasResult)
        {
            vertexShader =
                d3d_utils::compile_vertex_source_to_blob(shader,
                                                         m_vertexTarget);
        }

        if (!outShaderResult->pixelResult.hasResult)
        {
            pixelShader =
                d3d_utils::compile_pixel_source_to_blob(shader, m_pixelTarget);
        }

        compileBlobToFinalType(compileRequest,
                               vertexShader,
                               pixelShader,
                               outShaderResult);

        return true;
    }

private:
    std::unordered_map<uint32_t, VetexShaderType> m_drawVertexShaders;
    std::unordered_map<uint32_t, PixelShaderType> m_drawPixelShaders;

    ComPtr<DeviceType> m_device;
    const D3DCapabilities m_d3dCapabilities;

    const char* m_vertexTarget;
    const char* m_pixelTarget;
};

}; // namespace rive::gpu