// Copyright 2017 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.



#ifndef WEBGPU_CPP_H_
#define WEBGPU_CPP_H_

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <functional>
#include <string_view>
#include <type_traits>
#include <utility>

#include "webgpu/webgpu.h"
#include "webgpu/webgpu_cpp_chained_struct.h"
#include "webgpu/webgpu_enum_class_bitmasks.h"  // IWYU pragma: export

namespace wgpu {

static constexpr uint32_t kArrayLayerCountUndefined = WGPU_ARRAY_LAYER_COUNT_UNDEFINED;
static constexpr uint32_t kCopyStrideUndefined = WGPU_COPY_STRIDE_UNDEFINED;
static constexpr float kDepthClearValueUndefined = std::numeric_limits<float>::quiet_NaN();
static constexpr uint32_t kDepthSliceUndefined = WGPU_DEPTH_SLICE_UNDEFINED;
static constexpr uint32_t kLimitU32Undefined = WGPU_LIMIT_U32_UNDEFINED;
static constexpr uint64_t kLimitU64Undefined = WGPU_LIMIT_U64_UNDEFINED;
static constexpr uint32_t kMipLevelCountUndefined = WGPU_MIP_LEVEL_COUNT_UNDEFINED;
static constexpr uint32_t kQuerySetIndexUndefined = WGPU_QUERY_SET_INDEX_UNDEFINED;
static constexpr size_t kStrlen = WGPU_STRLEN;
static constexpr size_t kWholeMapSize = WGPU_WHOLE_MAP_SIZE;
static constexpr uint64_t kWholeSize = WGPU_WHOLE_SIZE;

enum class AdapterType : uint32_t {
    DiscreteGPU = WGPUAdapterType_DiscreteGPU,
    IntegratedGPU = WGPUAdapterType_IntegratedGPU,
    CPU = WGPUAdapterType_CPU,
    Unknown = WGPUAdapterType_Unknown,
};
static_assert(sizeof(AdapterType) == sizeof(WGPUAdapterType), "sizeof mismatch for AdapterType");
static_assert(alignof(AdapterType) == alignof(WGPUAdapterType), "alignof mismatch for AdapterType");

enum class AddressMode : uint32_t {
    Undefined = WGPUAddressMode_Undefined,
    ClampToEdge = WGPUAddressMode_ClampToEdge,
    Repeat = WGPUAddressMode_Repeat,
    MirrorRepeat = WGPUAddressMode_MirrorRepeat,
};
static_assert(sizeof(AddressMode) == sizeof(WGPUAddressMode), "sizeof mismatch for AddressMode");
static_assert(alignof(AddressMode) == alignof(WGPUAddressMode), "alignof mismatch for AddressMode");

enum class BackendType : uint32_t {
    Undefined = WGPUBackendType_Undefined,
    Null = WGPUBackendType_Null,
    WebGPU = WGPUBackendType_WebGPU,
    D3D11 = WGPUBackendType_D3D11,
    D3D12 = WGPUBackendType_D3D12,
    Metal = WGPUBackendType_Metal,
    Vulkan = WGPUBackendType_Vulkan,
    OpenGL = WGPUBackendType_OpenGL,
    OpenGLES = WGPUBackendType_OpenGLES,
};
static_assert(sizeof(BackendType) == sizeof(WGPUBackendType), "sizeof mismatch for BackendType");
static_assert(alignof(BackendType) == alignof(WGPUBackendType), "alignof mismatch for BackendType");

enum class BlendFactor : uint32_t {
    Undefined = WGPUBlendFactor_Undefined,
    Zero = WGPUBlendFactor_Zero,
    One = WGPUBlendFactor_One,
    Src = WGPUBlendFactor_Src,
    OneMinusSrc = WGPUBlendFactor_OneMinusSrc,
    SrcAlpha = WGPUBlendFactor_SrcAlpha,
    OneMinusSrcAlpha = WGPUBlendFactor_OneMinusSrcAlpha,
    Dst = WGPUBlendFactor_Dst,
    OneMinusDst = WGPUBlendFactor_OneMinusDst,
    DstAlpha = WGPUBlendFactor_DstAlpha,
    OneMinusDstAlpha = WGPUBlendFactor_OneMinusDstAlpha,
    SrcAlphaSaturated = WGPUBlendFactor_SrcAlphaSaturated,
    Constant = WGPUBlendFactor_Constant,
    OneMinusConstant = WGPUBlendFactor_OneMinusConstant,
    Src1 = WGPUBlendFactor_Src1,
    OneMinusSrc1 = WGPUBlendFactor_OneMinusSrc1,
    Src1Alpha = WGPUBlendFactor_Src1Alpha,
    OneMinusSrc1Alpha = WGPUBlendFactor_OneMinusSrc1Alpha,
};
static_assert(sizeof(BlendFactor) == sizeof(WGPUBlendFactor), "sizeof mismatch for BlendFactor");
static_assert(alignof(BlendFactor) == alignof(WGPUBlendFactor), "alignof mismatch for BlendFactor");

enum class BlendOperation : uint32_t {
    Undefined = WGPUBlendOperation_Undefined,
    Add = WGPUBlendOperation_Add,
    Subtract = WGPUBlendOperation_Subtract,
    ReverseSubtract = WGPUBlendOperation_ReverseSubtract,
    Min = WGPUBlendOperation_Min,
    Max = WGPUBlendOperation_Max,
};
static_assert(sizeof(BlendOperation) == sizeof(WGPUBlendOperation), "sizeof mismatch for BlendOperation");
static_assert(alignof(BlendOperation) == alignof(WGPUBlendOperation), "alignof mismatch for BlendOperation");

enum class BufferBindingType : uint32_t {
    BindingNotUsed = WGPUBufferBindingType_BindingNotUsed,
    Undefined = WGPUBufferBindingType_Undefined,
    Uniform = WGPUBufferBindingType_Uniform,
    Storage = WGPUBufferBindingType_Storage,
    ReadOnlyStorage = WGPUBufferBindingType_ReadOnlyStorage,
};
static_assert(sizeof(BufferBindingType) == sizeof(WGPUBufferBindingType), "sizeof mismatch for BufferBindingType");
static_assert(alignof(BufferBindingType) == alignof(WGPUBufferBindingType), "alignof mismatch for BufferBindingType");

enum class BufferMapState : uint32_t {
    Unmapped = WGPUBufferMapState_Unmapped,
    Pending = WGPUBufferMapState_Pending,
    Mapped = WGPUBufferMapState_Mapped,
};
static_assert(sizeof(BufferMapState) == sizeof(WGPUBufferMapState), "sizeof mismatch for BufferMapState");
static_assert(alignof(BufferMapState) == alignof(WGPUBufferMapState), "alignof mismatch for BufferMapState");

enum class CallbackMode : uint32_t {
    WaitAnyOnly = WGPUCallbackMode_WaitAnyOnly,
    AllowProcessEvents = WGPUCallbackMode_AllowProcessEvents,
    AllowSpontaneous = WGPUCallbackMode_AllowSpontaneous,
};
static_assert(sizeof(CallbackMode) == sizeof(WGPUCallbackMode), "sizeof mismatch for CallbackMode");
static_assert(alignof(CallbackMode) == alignof(WGPUCallbackMode), "alignof mismatch for CallbackMode");

enum class CompareFunction : uint32_t {
    Undefined = WGPUCompareFunction_Undefined,
    Never = WGPUCompareFunction_Never,
    Less = WGPUCompareFunction_Less,
    Equal = WGPUCompareFunction_Equal,
    LessEqual = WGPUCompareFunction_LessEqual,
    Greater = WGPUCompareFunction_Greater,
    NotEqual = WGPUCompareFunction_NotEqual,
    GreaterEqual = WGPUCompareFunction_GreaterEqual,
    Always = WGPUCompareFunction_Always,
};
static_assert(sizeof(CompareFunction) == sizeof(WGPUCompareFunction), "sizeof mismatch for CompareFunction");
static_assert(alignof(CompareFunction) == alignof(WGPUCompareFunction), "alignof mismatch for CompareFunction");

enum class CompilationInfoRequestStatus : uint32_t {
    Success = WGPUCompilationInfoRequestStatus_Success,
    CallbackCancelled = WGPUCompilationInfoRequestStatus_CallbackCancelled,
};
static_assert(sizeof(CompilationInfoRequestStatus) == sizeof(WGPUCompilationInfoRequestStatus), "sizeof mismatch for CompilationInfoRequestStatus");
static_assert(alignof(CompilationInfoRequestStatus) == alignof(WGPUCompilationInfoRequestStatus), "alignof mismatch for CompilationInfoRequestStatus");

enum class CompilationMessageType : uint32_t {
    Error = WGPUCompilationMessageType_Error,
    Warning = WGPUCompilationMessageType_Warning,
    Info = WGPUCompilationMessageType_Info,
};
static_assert(sizeof(CompilationMessageType) == sizeof(WGPUCompilationMessageType), "sizeof mismatch for CompilationMessageType");
static_assert(alignof(CompilationMessageType) == alignof(WGPUCompilationMessageType), "alignof mismatch for CompilationMessageType");

enum class ComponentSwizzle : uint32_t {
    Undefined = WGPUComponentSwizzle_Undefined,
    Zero = WGPUComponentSwizzle_Zero,
    One = WGPUComponentSwizzle_One,
    R = WGPUComponentSwizzle_R,
    G = WGPUComponentSwizzle_G,
    B = WGPUComponentSwizzle_B,
    A = WGPUComponentSwizzle_A,
};
static_assert(sizeof(ComponentSwizzle) == sizeof(WGPUComponentSwizzle), "sizeof mismatch for ComponentSwizzle");
static_assert(alignof(ComponentSwizzle) == alignof(WGPUComponentSwizzle), "alignof mismatch for ComponentSwizzle");

enum class CompositeAlphaMode : uint32_t {
    Auto = WGPUCompositeAlphaMode_Auto,
    Opaque = WGPUCompositeAlphaMode_Opaque,
    Premultiplied = WGPUCompositeAlphaMode_Premultiplied,
    Unpremultiplied = WGPUCompositeAlphaMode_Unpremultiplied,
    Inherit = WGPUCompositeAlphaMode_Inherit,
};
static_assert(sizeof(CompositeAlphaMode) == sizeof(WGPUCompositeAlphaMode), "sizeof mismatch for CompositeAlphaMode");
static_assert(alignof(CompositeAlphaMode) == alignof(WGPUCompositeAlphaMode), "alignof mismatch for CompositeAlphaMode");

enum class CreatePipelineAsyncStatus : uint32_t {
    Success = WGPUCreatePipelineAsyncStatus_Success,
    CallbackCancelled = WGPUCreatePipelineAsyncStatus_CallbackCancelled,
    ValidationError = WGPUCreatePipelineAsyncStatus_ValidationError,
    InternalError = WGPUCreatePipelineAsyncStatus_InternalError,
};
static_assert(sizeof(CreatePipelineAsyncStatus) == sizeof(WGPUCreatePipelineAsyncStatus), "sizeof mismatch for CreatePipelineAsyncStatus");
static_assert(alignof(CreatePipelineAsyncStatus) == alignof(WGPUCreatePipelineAsyncStatus), "alignof mismatch for CreatePipelineAsyncStatus");

enum class CullMode : uint32_t {
    Undefined = WGPUCullMode_Undefined,
    None = WGPUCullMode_None,
    Front = WGPUCullMode_Front,
    Back = WGPUCullMode_Back,
};
static_assert(sizeof(CullMode) == sizeof(WGPUCullMode), "sizeof mismatch for CullMode");
static_assert(alignof(CullMode) == alignof(WGPUCullMode), "alignof mismatch for CullMode");

enum class DeviceLostReason : uint32_t {
    Unknown = WGPUDeviceLostReason_Unknown,
    Destroyed = WGPUDeviceLostReason_Destroyed,
    CallbackCancelled = WGPUDeviceLostReason_CallbackCancelled,
    FailedCreation = WGPUDeviceLostReason_FailedCreation,
};
static_assert(sizeof(DeviceLostReason) == sizeof(WGPUDeviceLostReason), "sizeof mismatch for DeviceLostReason");
static_assert(alignof(DeviceLostReason) == alignof(WGPUDeviceLostReason), "alignof mismatch for DeviceLostReason");

enum class ErrorFilter : uint32_t {
    Validation = WGPUErrorFilter_Validation,
    OutOfMemory = WGPUErrorFilter_OutOfMemory,
    Internal = WGPUErrorFilter_Internal,
};
static_assert(sizeof(ErrorFilter) == sizeof(WGPUErrorFilter), "sizeof mismatch for ErrorFilter");
static_assert(alignof(ErrorFilter) == alignof(WGPUErrorFilter), "alignof mismatch for ErrorFilter");

enum class ErrorType : uint32_t {
    NoError = WGPUErrorType_NoError,
    Validation = WGPUErrorType_Validation,
    OutOfMemory = WGPUErrorType_OutOfMemory,
    Internal = WGPUErrorType_Internal,
    Unknown = WGPUErrorType_Unknown,
};
static_assert(sizeof(ErrorType) == sizeof(WGPUErrorType), "sizeof mismatch for ErrorType");
static_assert(alignof(ErrorType) == alignof(WGPUErrorType), "alignof mismatch for ErrorType");

enum class FeatureLevel : uint32_t {
    Undefined = WGPUFeatureLevel_Undefined,
    Compatibility = WGPUFeatureLevel_Compatibility,
    Core = WGPUFeatureLevel_Core,
};
static_assert(sizeof(FeatureLevel) == sizeof(WGPUFeatureLevel), "sizeof mismatch for FeatureLevel");
static_assert(alignof(FeatureLevel) == alignof(WGPUFeatureLevel), "alignof mismatch for FeatureLevel");

enum class FeatureName : uint32_t {
    CoreFeaturesAndLimits = WGPUFeatureName_CoreFeaturesAndLimits,
    DepthClipControl = WGPUFeatureName_DepthClipControl,
    Depth32FloatStencil8 = WGPUFeatureName_Depth32FloatStencil8,
    TextureCompressionBC = WGPUFeatureName_TextureCompressionBC,
    TextureCompressionBCSliced3D = WGPUFeatureName_TextureCompressionBCSliced3D,
    TextureCompressionETC2 = WGPUFeatureName_TextureCompressionETC2,
    TextureCompressionASTC = WGPUFeatureName_TextureCompressionASTC,
    TextureCompressionASTCSliced3D = WGPUFeatureName_TextureCompressionASTCSliced3D,
    TimestampQuery = WGPUFeatureName_TimestampQuery,
    IndirectFirstInstance = WGPUFeatureName_IndirectFirstInstance,
    ShaderF16 = WGPUFeatureName_ShaderF16,
    RG11B10UfloatRenderable = WGPUFeatureName_RG11B10UfloatRenderable,
    BGRA8UnormStorage = WGPUFeatureName_BGRA8UnormStorage,
    Float32Filterable = WGPUFeatureName_Float32Filterable,
    Float32Blendable = WGPUFeatureName_Float32Blendable,
    ClipDistances = WGPUFeatureName_ClipDistances,
    DualSourceBlending = WGPUFeatureName_DualSourceBlending,
    Subgroups = WGPUFeatureName_Subgroups,
    TextureFormatsTier1 = WGPUFeatureName_TextureFormatsTier1,
    TextureFormatsTier2 = WGPUFeatureName_TextureFormatsTier2,
    PrimitiveIndex = WGPUFeatureName_PrimitiveIndex,
    TextureComponentSwizzle = WGPUFeatureName_TextureComponentSwizzle,
    Unorm16TextureFormats = WGPUFeatureName_Unorm16TextureFormats,
    Snorm16TextureFormats = WGPUFeatureName_Snorm16TextureFormats,
    MultiDrawIndirect = WGPUFeatureName_MultiDrawIndirect,
};
static_assert(sizeof(FeatureName) == sizeof(WGPUFeatureName), "sizeof mismatch for FeatureName");
static_assert(alignof(FeatureName) == alignof(WGPUFeatureName), "alignof mismatch for FeatureName");

enum class FilterMode : uint32_t {
    Undefined = WGPUFilterMode_Undefined,
    Nearest = WGPUFilterMode_Nearest,
    Linear = WGPUFilterMode_Linear,
};
static_assert(sizeof(FilterMode) == sizeof(WGPUFilterMode), "sizeof mismatch for FilterMode");
static_assert(alignof(FilterMode) == alignof(WGPUFilterMode), "alignof mismatch for FilterMode");

enum class FrontFace : uint32_t {
    Undefined = WGPUFrontFace_Undefined,
    CCW = WGPUFrontFace_CCW,
    CW = WGPUFrontFace_CW,
};
static_assert(sizeof(FrontFace) == sizeof(WGPUFrontFace), "sizeof mismatch for FrontFace");
static_assert(alignof(FrontFace) == alignof(WGPUFrontFace), "alignof mismatch for FrontFace");

enum class IndexFormat : uint32_t {
    Undefined = WGPUIndexFormat_Undefined,
    Uint16 = WGPUIndexFormat_Uint16,
    Uint32 = WGPUIndexFormat_Uint32,
};
static_assert(sizeof(IndexFormat) == sizeof(WGPUIndexFormat), "sizeof mismatch for IndexFormat");
static_assert(alignof(IndexFormat) == alignof(WGPUIndexFormat), "alignof mismatch for IndexFormat");

enum class InstanceFeatureName : uint32_t {
    TimedWaitAny = WGPUInstanceFeatureName_TimedWaitAny,
    ShaderSourceSPIRV = WGPUInstanceFeatureName_ShaderSourceSPIRV,
    MultipleDevicesPerAdapter = WGPUInstanceFeatureName_MultipleDevicesPerAdapter,
};
static_assert(sizeof(InstanceFeatureName) == sizeof(WGPUInstanceFeatureName), "sizeof mismatch for InstanceFeatureName");
static_assert(alignof(InstanceFeatureName) == alignof(WGPUInstanceFeatureName), "alignof mismatch for InstanceFeatureName");

enum class LoadOp : uint32_t {
    Undefined = WGPULoadOp_Undefined,
    Load = WGPULoadOp_Load,
    Clear = WGPULoadOp_Clear,
};
static_assert(sizeof(LoadOp) == sizeof(WGPULoadOp), "sizeof mismatch for LoadOp");
static_assert(alignof(LoadOp) == alignof(WGPULoadOp), "alignof mismatch for LoadOp");

enum class MapAsyncStatus : uint32_t {
    Success = WGPUMapAsyncStatus_Success,
    CallbackCancelled = WGPUMapAsyncStatus_CallbackCancelled,
    Error = WGPUMapAsyncStatus_Error,
    Aborted = WGPUMapAsyncStatus_Aborted,
};
static_assert(sizeof(MapAsyncStatus) == sizeof(WGPUMapAsyncStatus), "sizeof mismatch for MapAsyncStatus");
static_assert(alignof(MapAsyncStatus) == alignof(WGPUMapAsyncStatus), "alignof mismatch for MapAsyncStatus");

enum class MipmapFilterMode : uint32_t {
    Undefined = WGPUMipmapFilterMode_Undefined,
    Nearest = WGPUMipmapFilterMode_Nearest,
    Linear = WGPUMipmapFilterMode_Linear,
};
static_assert(sizeof(MipmapFilterMode) == sizeof(WGPUMipmapFilterMode), "sizeof mismatch for MipmapFilterMode");
static_assert(alignof(MipmapFilterMode) == alignof(WGPUMipmapFilterMode), "alignof mismatch for MipmapFilterMode");

enum class PopErrorScopeStatus : uint32_t {
    Success = WGPUPopErrorScopeStatus_Success,
    CallbackCancelled = WGPUPopErrorScopeStatus_CallbackCancelled,
    Error = WGPUPopErrorScopeStatus_Error,
};
static_assert(sizeof(PopErrorScopeStatus) == sizeof(WGPUPopErrorScopeStatus), "sizeof mismatch for PopErrorScopeStatus");
static_assert(alignof(PopErrorScopeStatus) == alignof(WGPUPopErrorScopeStatus), "alignof mismatch for PopErrorScopeStatus");

enum class PowerPreference : uint32_t {
    Undefined = WGPUPowerPreference_Undefined,
    LowPower = WGPUPowerPreference_LowPower,
    HighPerformance = WGPUPowerPreference_HighPerformance,
};
static_assert(sizeof(PowerPreference) == sizeof(WGPUPowerPreference), "sizeof mismatch for PowerPreference");
static_assert(alignof(PowerPreference) == alignof(WGPUPowerPreference), "alignof mismatch for PowerPreference");

enum class PredefinedColorSpace : uint32_t {
    SRGB = WGPUPredefinedColorSpace_SRGB,
    DisplayP3 = WGPUPredefinedColorSpace_DisplayP3,
};
static_assert(sizeof(PredefinedColorSpace) == sizeof(WGPUPredefinedColorSpace), "sizeof mismatch for PredefinedColorSpace");
static_assert(alignof(PredefinedColorSpace) == alignof(WGPUPredefinedColorSpace), "alignof mismatch for PredefinedColorSpace");

enum class PresentMode : uint32_t {
    Undefined = WGPUPresentMode_Undefined,
    Fifo = WGPUPresentMode_Fifo,
    FifoRelaxed = WGPUPresentMode_FifoRelaxed,
    Immediate = WGPUPresentMode_Immediate,
    Mailbox = WGPUPresentMode_Mailbox,
};
static_assert(sizeof(PresentMode) == sizeof(WGPUPresentMode), "sizeof mismatch for PresentMode");
static_assert(alignof(PresentMode) == alignof(WGPUPresentMode), "alignof mismatch for PresentMode");

enum class PrimitiveTopology : uint32_t {
    Undefined = WGPUPrimitiveTopology_Undefined,
    PointList = WGPUPrimitiveTopology_PointList,
    LineList = WGPUPrimitiveTopology_LineList,
    LineStrip = WGPUPrimitiveTopology_LineStrip,
    TriangleList = WGPUPrimitiveTopology_TriangleList,
    TriangleStrip = WGPUPrimitiveTopology_TriangleStrip,
};
static_assert(sizeof(PrimitiveTopology) == sizeof(WGPUPrimitiveTopology), "sizeof mismatch for PrimitiveTopology");
static_assert(alignof(PrimitiveTopology) == alignof(WGPUPrimitiveTopology), "alignof mismatch for PrimitiveTopology");

enum class QueryType : uint32_t {
    Occlusion = WGPUQueryType_Occlusion,
    Timestamp = WGPUQueryType_Timestamp,
};
static_assert(sizeof(QueryType) == sizeof(WGPUQueryType), "sizeof mismatch for QueryType");
static_assert(alignof(QueryType) == alignof(WGPUQueryType), "alignof mismatch for QueryType");

enum class QueueWorkDoneStatus : uint32_t {
    Success = WGPUQueueWorkDoneStatus_Success,
    CallbackCancelled = WGPUQueueWorkDoneStatus_CallbackCancelled,
    Error = WGPUQueueWorkDoneStatus_Error,
};
static_assert(sizeof(QueueWorkDoneStatus) == sizeof(WGPUQueueWorkDoneStatus), "sizeof mismatch for QueueWorkDoneStatus");
static_assert(alignof(QueueWorkDoneStatus) == alignof(WGPUQueueWorkDoneStatus), "alignof mismatch for QueueWorkDoneStatus");

enum class RequestAdapterStatus : uint32_t {
    Success = WGPURequestAdapterStatus_Success,
    CallbackCancelled = WGPURequestAdapterStatus_CallbackCancelled,
    Unavailable = WGPURequestAdapterStatus_Unavailable,
    Error = WGPURequestAdapterStatus_Error,
};
static_assert(sizeof(RequestAdapterStatus) == sizeof(WGPURequestAdapterStatus), "sizeof mismatch for RequestAdapterStatus");
static_assert(alignof(RequestAdapterStatus) == alignof(WGPURequestAdapterStatus), "alignof mismatch for RequestAdapterStatus");

enum class RequestDeviceStatus : uint32_t {
    Success = WGPURequestDeviceStatus_Success,
    CallbackCancelled = WGPURequestDeviceStatus_CallbackCancelled,
    Error = WGPURequestDeviceStatus_Error,
};
static_assert(sizeof(RequestDeviceStatus) == sizeof(WGPURequestDeviceStatus), "sizeof mismatch for RequestDeviceStatus");
static_assert(alignof(RequestDeviceStatus) == alignof(WGPURequestDeviceStatus), "alignof mismatch for RequestDeviceStatus");

enum class SamplerBindingType : uint32_t {
    BindingNotUsed = WGPUSamplerBindingType_BindingNotUsed,
    Undefined = WGPUSamplerBindingType_Undefined,
    Filtering = WGPUSamplerBindingType_Filtering,
    NonFiltering = WGPUSamplerBindingType_NonFiltering,
    Comparison = WGPUSamplerBindingType_Comparison,
};
static_assert(sizeof(SamplerBindingType) == sizeof(WGPUSamplerBindingType), "sizeof mismatch for SamplerBindingType");
static_assert(alignof(SamplerBindingType) == alignof(WGPUSamplerBindingType), "alignof mismatch for SamplerBindingType");

enum class Status : uint32_t {
    Success = WGPUStatus_Success,
    Error = WGPUStatus_Error,
};
static_assert(sizeof(Status) == sizeof(WGPUStatus), "sizeof mismatch for Status");
static_assert(alignof(Status) == alignof(WGPUStatus), "alignof mismatch for Status");

enum class StencilOperation : uint32_t {
    Undefined = WGPUStencilOperation_Undefined,
    Keep = WGPUStencilOperation_Keep,
    Zero = WGPUStencilOperation_Zero,
    Replace = WGPUStencilOperation_Replace,
    Invert = WGPUStencilOperation_Invert,
    IncrementClamp = WGPUStencilOperation_IncrementClamp,
    DecrementClamp = WGPUStencilOperation_DecrementClamp,
    IncrementWrap = WGPUStencilOperation_IncrementWrap,
    DecrementWrap = WGPUStencilOperation_DecrementWrap,
};
static_assert(sizeof(StencilOperation) == sizeof(WGPUStencilOperation), "sizeof mismatch for StencilOperation");
static_assert(alignof(StencilOperation) == alignof(WGPUStencilOperation), "alignof mismatch for StencilOperation");

enum class StorageTextureAccess : uint32_t {
    BindingNotUsed = WGPUStorageTextureAccess_BindingNotUsed,
    Undefined = WGPUStorageTextureAccess_Undefined,
    WriteOnly = WGPUStorageTextureAccess_WriteOnly,
    ReadOnly = WGPUStorageTextureAccess_ReadOnly,
    ReadWrite = WGPUStorageTextureAccess_ReadWrite,
};
static_assert(sizeof(StorageTextureAccess) == sizeof(WGPUStorageTextureAccess), "sizeof mismatch for StorageTextureAccess");
static_assert(alignof(StorageTextureAccess) == alignof(WGPUStorageTextureAccess), "alignof mismatch for StorageTextureAccess");

enum class StoreOp : uint32_t {
    Undefined = WGPUStoreOp_Undefined,
    Store = WGPUStoreOp_Store,
    Discard = WGPUStoreOp_Discard,
};
static_assert(sizeof(StoreOp) == sizeof(WGPUStoreOp), "sizeof mismatch for StoreOp");
static_assert(alignof(StoreOp) == alignof(WGPUStoreOp), "alignof mismatch for StoreOp");

enum class SType : uint32_t {
    ShaderSourceSPIRV = WGPUSType_ShaderSourceSPIRV,
    ShaderSourceWGSL = WGPUSType_ShaderSourceWGSL,
    RenderPassMaxDrawCount = WGPUSType_RenderPassMaxDrawCount,
    SurfaceSourceMetalLayer = WGPUSType_SurfaceSourceMetalLayer,
    SurfaceSourceWindowsHWND = WGPUSType_SurfaceSourceWindowsHWND,
    SurfaceSourceXlibWindow = WGPUSType_SurfaceSourceXlibWindow,
    SurfaceSourceWaylandSurface = WGPUSType_SurfaceSourceWaylandSurface,
    SurfaceSourceAndroidNativeWindow = WGPUSType_SurfaceSourceAndroidNativeWindow,
    SurfaceSourceXCBWindow = WGPUSType_SurfaceSourceXCBWindow,
    SurfaceColorManagement = WGPUSType_SurfaceColorManagement,
    RequestAdapterWebXROptions = WGPUSType_RequestAdapterWebXROptions,
    TextureComponentSwizzleDescriptor = WGPUSType_TextureComponentSwizzleDescriptor,
    CompatibilityModeLimits = WGPUSType_CompatibilityModeLimits,
    TextureBindingViewDimensionDescriptor = WGPUSType_TextureBindingViewDimensionDescriptor,
    EmscriptenSurfaceSourceCanvasHTMLSelector = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector,
    DawnCompilationMessageUtf16 = WGPUSType_DawnCompilationMessageUtf16,
};
static_assert(sizeof(SType) == sizeof(WGPUSType), "sizeof mismatch for SType");
static_assert(alignof(SType) == alignof(WGPUSType), "alignof mismatch for SType");

enum class SurfaceGetCurrentTextureStatus : uint32_t {
    SuccessOptimal = WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal,
    SuccessSuboptimal = WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal,
    Timeout = WGPUSurfaceGetCurrentTextureStatus_Timeout,
    Outdated = WGPUSurfaceGetCurrentTextureStatus_Outdated,
    Lost = WGPUSurfaceGetCurrentTextureStatus_Lost,
    Error = WGPUSurfaceGetCurrentTextureStatus_Error,
};
static_assert(sizeof(SurfaceGetCurrentTextureStatus) == sizeof(WGPUSurfaceGetCurrentTextureStatus), "sizeof mismatch for SurfaceGetCurrentTextureStatus");
static_assert(alignof(SurfaceGetCurrentTextureStatus) == alignof(WGPUSurfaceGetCurrentTextureStatus), "alignof mismatch for SurfaceGetCurrentTextureStatus");

enum class TextureAspect : uint32_t {
    Undefined = WGPUTextureAspect_Undefined,
    All = WGPUTextureAspect_All,
    StencilOnly = WGPUTextureAspect_StencilOnly,
    DepthOnly = WGPUTextureAspect_DepthOnly,
};
static_assert(sizeof(TextureAspect) == sizeof(WGPUTextureAspect), "sizeof mismatch for TextureAspect");
static_assert(alignof(TextureAspect) == alignof(WGPUTextureAspect), "alignof mismatch for TextureAspect");

enum class TextureDimension : uint32_t {
    Undefined = WGPUTextureDimension_Undefined,
    e1D = WGPUTextureDimension_1D,
    e2D = WGPUTextureDimension_2D,
    e3D = WGPUTextureDimension_3D,
};
static_assert(sizeof(TextureDimension) == sizeof(WGPUTextureDimension), "sizeof mismatch for TextureDimension");
static_assert(alignof(TextureDimension) == alignof(WGPUTextureDimension), "alignof mismatch for TextureDimension");

enum class TextureFormat : uint32_t {
    Undefined = WGPUTextureFormat_Undefined,
    R8Unorm = WGPUTextureFormat_R8Unorm,
    R8Snorm = WGPUTextureFormat_R8Snorm,
    R8Uint = WGPUTextureFormat_R8Uint,
    R8Sint = WGPUTextureFormat_R8Sint,
    R16Unorm = WGPUTextureFormat_R16Unorm,
    R16Snorm = WGPUTextureFormat_R16Snorm,
    R16Uint = WGPUTextureFormat_R16Uint,
    R16Sint = WGPUTextureFormat_R16Sint,
    R16Float = WGPUTextureFormat_R16Float,
    RG8Unorm = WGPUTextureFormat_RG8Unorm,
    RG8Snorm = WGPUTextureFormat_RG8Snorm,
    RG8Uint = WGPUTextureFormat_RG8Uint,
    RG8Sint = WGPUTextureFormat_RG8Sint,
    R32Float = WGPUTextureFormat_R32Float,
    R32Uint = WGPUTextureFormat_R32Uint,
    R32Sint = WGPUTextureFormat_R32Sint,
    RG16Unorm = WGPUTextureFormat_RG16Unorm,
    RG16Snorm = WGPUTextureFormat_RG16Snorm,
    RG16Uint = WGPUTextureFormat_RG16Uint,
    RG16Sint = WGPUTextureFormat_RG16Sint,
    RG16Float = WGPUTextureFormat_RG16Float,
    RGBA8Unorm = WGPUTextureFormat_RGBA8Unorm,
    RGBA8UnormSrgb = WGPUTextureFormat_RGBA8UnormSrgb,
    RGBA8Snorm = WGPUTextureFormat_RGBA8Snorm,
    RGBA8Uint = WGPUTextureFormat_RGBA8Uint,
    RGBA8Sint = WGPUTextureFormat_RGBA8Sint,
    BGRA8Unorm = WGPUTextureFormat_BGRA8Unorm,
    BGRA8UnormSrgb = WGPUTextureFormat_BGRA8UnormSrgb,
    RGB10A2Uint = WGPUTextureFormat_RGB10A2Uint,
    RGB10A2Unorm = WGPUTextureFormat_RGB10A2Unorm,
    RG11B10Ufloat = WGPUTextureFormat_RG11B10Ufloat,
    RGB9E5Ufloat = WGPUTextureFormat_RGB9E5Ufloat,
    RG32Float = WGPUTextureFormat_RG32Float,
    RG32Uint = WGPUTextureFormat_RG32Uint,
    RG32Sint = WGPUTextureFormat_RG32Sint,
    RGBA16Unorm = WGPUTextureFormat_RGBA16Unorm,
    RGBA16Snorm = WGPUTextureFormat_RGBA16Snorm,
    RGBA16Uint = WGPUTextureFormat_RGBA16Uint,
    RGBA16Sint = WGPUTextureFormat_RGBA16Sint,
    RGBA16Float = WGPUTextureFormat_RGBA16Float,
    RGBA32Float = WGPUTextureFormat_RGBA32Float,
    RGBA32Uint = WGPUTextureFormat_RGBA32Uint,
    RGBA32Sint = WGPUTextureFormat_RGBA32Sint,
    Stencil8 = WGPUTextureFormat_Stencil8,
    Depth16Unorm = WGPUTextureFormat_Depth16Unorm,
    Depth24Plus = WGPUTextureFormat_Depth24Plus,
    Depth24PlusStencil8 = WGPUTextureFormat_Depth24PlusStencil8,
    Depth32Float = WGPUTextureFormat_Depth32Float,
    Depth32FloatStencil8 = WGPUTextureFormat_Depth32FloatStencil8,
    BC1RGBAUnorm = WGPUTextureFormat_BC1RGBAUnorm,
    BC1RGBAUnormSrgb = WGPUTextureFormat_BC1RGBAUnormSrgb,
    BC2RGBAUnorm = WGPUTextureFormat_BC2RGBAUnorm,
    BC2RGBAUnormSrgb = WGPUTextureFormat_BC2RGBAUnormSrgb,
    BC3RGBAUnorm = WGPUTextureFormat_BC3RGBAUnorm,
    BC3RGBAUnormSrgb = WGPUTextureFormat_BC3RGBAUnormSrgb,
    BC4RUnorm = WGPUTextureFormat_BC4RUnorm,
    BC4RSnorm = WGPUTextureFormat_BC4RSnorm,
    BC5RGUnorm = WGPUTextureFormat_BC5RGUnorm,
    BC5RGSnorm = WGPUTextureFormat_BC5RGSnorm,
    BC6HRGBUfloat = WGPUTextureFormat_BC6HRGBUfloat,
    BC6HRGBFloat = WGPUTextureFormat_BC6HRGBFloat,
    BC7RGBAUnorm = WGPUTextureFormat_BC7RGBAUnorm,
    BC7RGBAUnormSrgb = WGPUTextureFormat_BC7RGBAUnormSrgb,
    ETC2RGB8Unorm = WGPUTextureFormat_ETC2RGB8Unorm,
    ETC2RGB8UnormSrgb = WGPUTextureFormat_ETC2RGB8UnormSrgb,
    ETC2RGB8A1Unorm = WGPUTextureFormat_ETC2RGB8A1Unorm,
    ETC2RGB8A1UnormSrgb = WGPUTextureFormat_ETC2RGB8A1UnormSrgb,
    ETC2RGBA8Unorm = WGPUTextureFormat_ETC2RGBA8Unorm,
    ETC2RGBA8UnormSrgb = WGPUTextureFormat_ETC2RGBA8UnormSrgb,
    EACR11Unorm = WGPUTextureFormat_EACR11Unorm,
    EACR11Snorm = WGPUTextureFormat_EACR11Snorm,
    EACRG11Unorm = WGPUTextureFormat_EACRG11Unorm,
    EACRG11Snorm = WGPUTextureFormat_EACRG11Snorm,
    ASTC4x4Unorm = WGPUTextureFormat_ASTC4x4Unorm,
    ASTC4x4UnormSrgb = WGPUTextureFormat_ASTC4x4UnormSrgb,
    ASTC5x4Unorm = WGPUTextureFormat_ASTC5x4Unorm,
    ASTC5x4UnormSrgb = WGPUTextureFormat_ASTC5x4UnormSrgb,
    ASTC5x5Unorm = WGPUTextureFormat_ASTC5x5Unorm,
    ASTC5x5UnormSrgb = WGPUTextureFormat_ASTC5x5UnormSrgb,
    ASTC6x5Unorm = WGPUTextureFormat_ASTC6x5Unorm,
    ASTC6x5UnormSrgb = WGPUTextureFormat_ASTC6x5UnormSrgb,
    ASTC6x6Unorm = WGPUTextureFormat_ASTC6x6Unorm,
    ASTC6x6UnormSrgb = WGPUTextureFormat_ASTC6x6UnormSrgb,
    ASTC8x5Unorm = WGPUTextureFormat_ASTC8x5Unorm,
    ASTC8x5UnormSrgb = WGPUTextureFormat_ASTC8x5UnormSrgb,
    ASTC8x6Unorm = WGPUTextureFormat_ASTC8x6Unorm,
    ASTC8x6UnormSrgb = WGPUTextureFormat_ASTC8x6UnormSrgb,
    ASTC8x8Unorm = WGPUTextureFormat_ASTC8x8Unorm,
    ASTC8x8UnormSrgb = WGPUTextureFormat_ASTC8x8UnormSrgb,
    ASTC10x5Unorm = WGPUTextureFormat_ASTC10x5Unorm,
    ASTC10x5UnormSrgb = WGPUTextureFormat_ASTC10x5UnormSrgb,
    ASTC10x6Unorm = WGPUTextureFormat_ASTC10x6Unorm,
    ASTC10x6UnormSrgb = WGPUTextureFormat_ASTC10x6UnormSrgb,
    ASTC10x8Unorm = WGPUTextureFormat_ASTC10x8Unorm,
    ASTC10x8UnormSrgb = WGPUTextureFormat_ASTC10x8UnormSrgb,
    ASTC10x10Unorm = WGPUTextureFormat_ASTC10x10Unorm,
    ASTC10x10UnormSrgb = WGPUTextureFormat_ASTC10x10UnormSrgb,
    ASTC12x10Unorm = WGPUTextureFormat_ASTC12x10Unorm,
    ASTC12x10UnormSrgb = WGPUTextureFormat_ASTC12x10UnormSrgb,
    ASTC12x12Unorm = WGPUTextureFormat_ASTC12x12Unorm,
    ASTC12x12UnormSrgb = WGPUTextureFormat_ASTC12x12UnormSrgb,
};
static_assert(sizeof(TextureFormat) == sizeof(WGPUTextureFormat), "sizeof mismatch for TextureFormat");
static_assert(alignof(TextureFormat) == alignof(WGPUTextureFormat), "alignof mismatch for TextureFormat");

enum class TextureSampleType : uint32_t {
    BindingNotUsed = WGPUTextureSampleType_BindingNotUsed,
    Undefined = WGPUTextureSampleType_Undefined,
    Float = WGPUTextureSampleType_Float,
    UnfilterableFloat = WGPUTextureSampleType_UnfilterableFloat,
    Depth = WGPUTextureSampleType_Depth,
    Sint = WGPUTextureSampleType_Sint,
    Uint = WGPUTextureSampleType_Uint,
};
static_assert(sizeof(TextureSampleType) == sizeof(WGPUTextureSampleType), "sizeof mismatch for TextureSampleType");
static_assert(alignof(TextureSampleType) == alignof(WGPUTextureSampleType), "alignof mismatch for TextureSampleType");

enum class TextureViewDimension : uint32_t {
    Undefined = WGPUTextureViewDimension_Undefined,
    e1D = WGPUTextureViewDimension_1D,
    e2D = WGPUTextureViewDimension_2D,
    e2DArray = WGPUTextureViewDimension_2DArray,
    Cube = WGPUTextureViewDimension_Cube,
    CubeArray = WGPUTextureViewDimension_CubeArray,
    e3D = WGPUTextureViewDimension_3D,
};
static_assert(sizeof(TextureViewDimension) == sizeof(WGPUTextureViewDimension), "sizeof mismatch for TextureViewDimension");
static_assert(alignof(TextureViewDimension) == alignof(WGPUTextureViewDimension), "alignof mismatch for TextureViewDimension");

enum class ToneMappingMode : uint32_t {
    Standard = WGPUToneMappingMode_Standard,
    Extended = WGPUToneMappingMode_Extended,
};
static_assert(sizeof(ToneMappingMode) == sizeof(WGPUToneMappingMode), "sizeof mismatch for ToneMappingMode");
static_assert(alignof(ToneMappingMode) == alignof(WGPUToneMappingMode), "alignof mismatch for ToneMappingMode");

enum class VertexFormat : uint32_t {
    Uint8 = WGPUVertexFormat_Uint8,
    Uint8x2 = WGPUVertexFormat_Uint8x2,
    Uint8x4 = WGPUVertexFormat_Uint8x4,
    Sint8 = WGPUVertexFormat_Sint8,
    Sint8x2 = WGPUVertexFormat_Sint8x2,
    Sint8x4 = WGPUVertexFormat_Sint8x4,
    Unorm8 = WGPUVertexFormat_Unorm8,
    Unorm8x2 = WGPUVertexFormat_Unorm8x2,
    Unorm8x4 = WGPUVertexFormat_Unorm8x4,
    Snorm8 = WGPUVertexFormat_Snorm8,
    Snorm8x2 = WGPUVertexFormat_Snorm8x2,
    Snorm8x4 = WGPUVertexFormat_Snorm8x4,
    Uint16 = WGPUVertexFormat_Uint16,
    Uint16x2 = WGPUVertexFormat_Uint16x2,
    Uint16x4 = WGPUVertexFormat_Uint16x4,
    Sint16 = WGPUVertexFormat_Sint16,
    Sint16x2 = WGPUVertexFormat_Sint16x2,
    Sint16x4 = WGPUVertexFormat_Sint16x4,
    Unorm16 = WGPUVertexFormat_Unorm16,
    Unorm16x2 = WGPUVertexFormat_Unorm16x2,
    Unorm16x4 = WGPUVertexFormat_Unorm16x4,
    Snorm16 = WGPUVertexFormat_Snorm16,
    Snorm16x2 = WGPUVertexFormat_Snorm16x2,
    Snorm16x4 = WGPUVertexFormat_Snorm16x4,
    Float16 = WGPUVertexFormat_Float16,
    Float16x2 = WGPUVertexFormat_Float16x2,
    Float16x4 = WGPUVertexFormat_Float16x4,
    Float32 = WGPUVertexFormat_Float32,
    Float32x2 = WGPUVertexFormat_Float32x2,
    Float32x3 = WGPUVertexFormat_Float32x3,
    Float32x4 = WGPUVertexFormat_Float32x4,
    Uint32 = WGPUVertexFormat_Uint32,
    Uint32x2 = WGPUVertexFormat_Uint32x2,
    Uint32x3 = WGPUVertexFormat_Uint32x3,
    Uint32x4 = WGPUVertexFormat_Uint32x4,
    Sint32 = WGPUVertexFormat_Sint32,
    Sint32x2 = WGPUVertexFormat_Sint32x2,
    Sint32x3 = WGPUVertexFormat_Sint32x3,
    Sint32x4 = WGPUVertexFormat_Sint32x4,
    Unorm10_10_10_2 = WGPUVertexFormat_Unorm10_10_10_2,
    Unorm8x4BGRA = WGPUVertexFormat_Unorm8x4BGRA,
};
static_assert(sizeof(VertexFormat) == sizeof(WGPUVertexFormat), "sizeof mismatch for VertexFormat");
static_assert(alignof(VertexFormat) == alignof(WGPUVertexFormat), "alignof mismatch for VertexFormat");

enum class VertexStepMode : uint32_t {
    Undefined = WGPUVertexStepMode_Undefined,
    Vertex = WGPUVertexStepMode_Vertex,
    Instance = WGPUVertexStepMode_Instance,
};
static_assert(sizeof(VertexStepMode) == sizeof(WGPUVertexStepMode), "sizeof mismatch for VertexStepMode");
static_assert(alignof(VertexStepMode) == alignof(WGPUVertexStepMode), "alignof mismatch for VertexStepMode");

enum class WaitStatus : uint32_t {
    Success = WGPUWaitStatus_Success,
    TimedOut = WGPUWaitStatus_TimedOut,
    Error = WGPUWaitStatus_Error,
};
static_assert(sizeof(WaitStatus) == sizeof(WGPUWaitStatus), "sizeof mismatch for WaitStatus");
static_assert(alignof(WaitStatus) == alignof(WGPUWaitStatus), "alignof mismatch for WaitStatus");

enum class WGSLLanguageFeatureName : uint32_t {
    ReadonlyAndReadwriteStorageTextures = WGPUWGSLLanguageFeatureName_ReadonlyAndReadwriteStorageTextures,
    Packed4x8IntegerDotProduct = WGPUWGSLLanguageFeatureName_Packed4x8IntegerDotProduct,
    UnrestrictedPointerParameters = WGPUWGSLLanguageFeatureName_UnrestrictedPointerParameters,
    PointerCompositeAccess = WGPUWGSLLanguageFeatureName_PointerCompositeAccess,
};
static_assert(sizeof(WGSLLanguageFeatureName) == sizeof(WGPUWGSLLanguageFeatureName), "sizeof mismatch for WGSLLanguageFeatureName");
static_assert(alignof(WGSLLanguageFeatureName) == alignof(WGPUWGSLLanguageFeatureName), "alignof mismatch for WGSLLanguageFeatureName");


enum class BufferUsage : uint64_t {
    None = WGPUBufferUsage_None,
    MapRead = WGPUBufferUsage_MapRead,
    MapWrite = WGPUBufferUsage_MapWrite,
    CopySrc = WGPUBufferUsage_CopySrc,
    CopyDst = WGPUBufferUsage_CopyDst,
    Index = WGPUBufferUsage_Index,
    Vertex = WGPUBufferUsage_Vertex,
    Uniform = WGPUBufferUsage_Uniform,
    Storage = WGPUBufferUsage_Storage,
    Indirect = WGPUBufferUsage_Indirect,
    QueryResolve = WGPUBufferUsage_QueryResolve,
};
static_assert(sizeof(BufferUsage) == sizeof(WGPUBufferUsage), "sizeof mismatch for BufferUsage");
static_assert(alignof(BufferUsage) == alignof(WGPUBufferUsage), "alignof mismatch for BufferUsage");

enum class ColorWriteMask : uint64_t {
    None = WGPUColorWriteMask_None,
    Red = WGPUColorWriteMask_Red,
    Green = WGPUColorWriteMask_Green,
    Blue = WGPUColorWriteMask_Blue,
    Alpha = WGPUColorWriteMask_Alpha,
    All = WGPUColorWriteMask_All,
};
static_assert(sizeof(ColorWriteMask) == sizeof(WGPUColorWriteMask), "sizeof mismatch for ColorWriteMask");
static_assert(alignof(ColorWriteMask) == alignof(WGPUColorWriteMask), "alignof mismatch for ColorWriteMask");

enum class MapMode : uint64_t {
    None = WGPUMapMode_None,
    Read = WGPUMapMode_Read,
    Write = WGPUMapMode_Write,
};
static_assert(sizeof(MapMode) == sizeof(WGPUMapMode), "sizeof mismatch for MapMode");
static_assert(alignof(MapMode) == alignof(WGPUMapMode), "alignof mismatch for MapMode");

enum class ShaderStage : uint64_t {
    None = WGPUShaderStage_None,
    Vertex = WGPUShaderStage_Vertex,
    Fragment = WGPUShaderStage_Fragment,
    Compute = WGPUShaderStage_Compute,
};
static_assert(sizeof(ShaderStage) == sizeof(WGPUShaderStage), "sizeof mismatch for ShaderStage");
static_assert(alignof(ShaderStage) == alignof(WGPUShaderStage), "alignof mismatch for ShaderStage");

enum class TextureUsage : uint64_t {
    None = WGPUTextureUsage_None,
    CopySrc = WGPUTextureUsage_CopySrc,
    CopyDst = WGPUTextureUsage_CopyDst,
    TextureBinding = WGPUTextureUsage_TextureBinding,
    StorageBinding = WGPUTextureUsage_StorageBinding,
    RenderAttachment = WGPUTextureUsage_RenderAttachment,
};
static_assert(sizeof(TextureUsage) == sizeof(WGPUTextureUsage), "sizeof mismatch for TextureUsage");
static_assert(alignof(TextureUsage) == alignof(WGPUTextureUsage), "alignof mismatch for TextureUsage");


// TODO(crbug.com/42241461): Update these to not be using the C callback types, and instead be
// defined using C++ types instead. Note that when we remove these, the C++ callback info types
// should also all be removed as they will no longer be necessary given the C++ templated
// functions calls and setter utilities.
using Proc = WGPUProc;

// Special class for booleans in order to allow implicit conversions.
class Bool {
  public:
    constexpr Bool() = default;
    // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
    constexpr Bool(bool value) : mValue(static_cast<WGPUBool>(value)) {}
    // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
    Bool(WGPUBool value): mValue(value) {}

    constexpr operator bool() const { return static_cast<bool>(mValue); }

  private:
    friend struct std::hash<Bool>;
    // Default to false.
    WGPUBool mValue = static_cast<WGPUBool>(false);
};

// Special class for optional booleans in order to allow conversions.
class OptionalBool {
  public:
    constexpr OptionalBool() = default;
    // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
    constexpr OptionalBool(bool value) : mValue(static_cast<WGPUOptionalBool>(value)) {}
    // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
    constexpr OptionalBool(std::optional<bool> value) :
        mValue(value ? static_cast<WGPUOptionalBool>(*value) : WGPUOptionalBool_Undefined) {}
    // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
    constexpr OptionalBool(WGPUOptionalBool value): mValue(value) {}

    // Define the values that are equivalent to the enums.
    static const OptionalBool False;
    static const OptionalBool True;
    static const OptionalBool Undefined;

    // Assignment operators.
    OptionalBool& operator=(const bool& value) {
        mValue = static_cast<WGPUOptionalBool>(value);
        return *this;
    }
    OptionalBool& operator=(const std::optional<bool>& value) {
        mValue = value ? static_cast<WGPUOptionalBool>(*value) : WGPUOptionalBool_Undefined;
        return *this;
    }
    OptionalBool& operator=(const WGPUOptionalBool& value) {
        mValue = value;
        return *this;
    }

    // Conversion functions.
    operator WGPUOptionalBool() const { return mValue; }
    operator std::optional<bool>() const {
        if (mValue == WGPUOptionalBool_Undefined) {
            return std::nullopt;
        }
        return static_cast<bool>(mValue);
    }

    // Comparison functions.
    friend bool operator==(const OptionalBool& lhs, const OptionalBool& rhs) {
        return lhs.mValue == rhs.mValue;
    }
    friend bool operator!=(const OptionalBool& lhs, const OptionalBool& rhs) {
        return lhs.mValue != rhs.mValue;
    }

  private:
    friend struct std::hash<OptionalBool>;
    // Default to undefined.
    WGPUOptionalBool mValue = WGPUOptionalBool_Undefined;
};
inline const OptionalBool OptionalBool::False = OptionalBool(WGPUOptionalBool_False);
inline const OptionalBool OptionalBool::True = OptionalBool(WGPUOptionalBool_True);
inline const OptionalBool OptionalBool::Undefined = OptionalBool(WGPUOptionalBool_Undefined);

// Helper class to wrap Status which allows implicit conversion to bool.
// Used while callers switch to checking the Status enum instead of booleans.
// TODO(crbug.com/42241199): Remove when all callers check the enum.
struct ConvertibleStatus {
    // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
    constexpr ConvertibleStatus(Status status) : status(status) {}
    // NOLINTNEXTLINE(runtime/explicit) allow implicit conversion
    constexpr operator bool() const {
        return status == Status::Success;
    }
    // NOLINTNEXTLINE(runtime/explicit) allow implicit conversion
    constexpr operator Status() const {
        return status;
    }
    Status status;
};

template<typename Derived, typename CType>
class ObjectBase {
  public:
    ObjectBase() = default;
    ObjectBase(CType handle): mHandle(handle) {
        if (mHandle) Derived::WGPUAddRef(mHandle);
    }
    ~ObjectBase() {
        if (mHandle) Derived::WGPURelease(mHandle);
    }

    ObjectBase(ObjectBase const& other)
        : ObjectBase(other.Get()) {
    }
    Derived& operator=(ObjectBase const& other) {
        if (&other != this) {
            if (mHandle) Derived::WGPURelease(mHandle);
            mHandle = other.mHandle;
            if (mHandle) Derived::WGPUAddRef(mHandle);
        }

        return static_cast<Derived&>(*this);
    }

    ObjectBase(ObjectBase&& other) {
        mHandle = other.mHandle;
        other.mHandle = 0;
    }
    Derived& operator=(ObjectBase&& other) {
        if (&other != this) {
            if (mHandle) Derived::WGPURelease(mHandle);
            mHandle = other.mHandle;
            other.mHandle = 0;
        }

        return static_cast<Derived&>(*this);
    }

    ObjectBase(std::nullptr_t) {}
    Derived& operator=(std::nullptr_t) {
        if (mHandle != nullptr) {
            Derived::WGPURelease(mHandle);
            mHandle = nullptr;
        }
        return static_cast<Derived&>(*this);
    }

    bool operator==(std::nullptr_t) const {
        return mHandle == nullptr;
    }
    bool operator!=(std::nullptr_t) const {
        return mHandle != nullptr;
    }

    explicit operator bool() const {
        return mHandle != nullptr;
    }
    CType Get() const {
        return mHandle;
    }
    CType MoveToCHandle() {
        CType result = mHandle;
        mHandle = 0;
        return result;
    }
    static Derived Acquire(CType handle) {
        Derived result;
        result.mHandle = handle;
        return result;
    }

  protected:
    CType mHandle = nullptr;
};

class Adapter;
class BindGroup;
class BindGroupLayout;
class Buffer;
class CommandBuffer;
class CommandEncoder;
class ComputePassEncoder;
class ComputePipeline;
class Device;
class Instance;
class PipelineLayout;
class QuerySet;
class Queue;
class RenderBundle;
class RenderBundleEncoder;
class RenderPassEncoder;
class RenderPipeline;
class Sampler;
class ShaderModule;
class Surface;
class Texture;
class TextureView;

struct StringView;
struct AdapterInfo;
struct BindGroupEntry;
struct BlendComponent;
struct BufferBindingLayout;
struct BufferDescriptor;
struct Color;
struct CommandBufferDescriptor;
struct CommandEncoderDescriptor;
struct CompatibilityModeLimits;
struct ConstantEntry;
struct DawnCompilationMessageUtf16;
struct EmscriptenSurfaceSourceCanvasHTMLSelector;
struct Extent3D;
struct Future;
struct InstanceLimits;
struct INTERNAL_HAVE_EMDAWNWEBGPU_HEADER;
struct MultisampleState;
struct Origin3D;
struct PassTimestampWrites;
struct PipelineLayoutDescriptor;
struct PrimitiveState;
struct QuerySetDescriptor;
struct QueueDescriptor;
struct RenderBundleDescriptor;
struct RenderBundleEncoderDescriptor;
struct RenderPassDepthStencilAttachment;
struct RenderPassMaxDrawCount;
struct RequestAdapterWebXROptions;
struct SamplerBindingLayout;
struct SamplerDescriptor;
struct ShaderSourceSPIRV;
struct ShaderSourceWGSL;
struct StencilFaceState;
struct StorageTextureBindingLayout;
struct SupportedFeatures;
struct SupportedInstanceFeatures;
struct SupportedWGSLLanguageFeatures;
struct SurfaceCapabilities;
struct SurfaceColorManagement;
struct SurfaceConfiguration;
struct SurfaceTexture;
struct TexelCopyBufferLayout;
struct TextureBindingLayout;
struct TextureBindingViewDimensionDescriptor;
struct TextureComponentSwizzle;
struct VertexAttribute;
struct BindGroupDescriptor;
struct BindGroupLayoutEntry;
struct BlendState;
struct CompilationMessage;
struct ComputePassDescriptor;
struct ComputeState;
struct DepthStencilState;
struct FutureWaitInfo;
struct InstanceDescriptor;
struct Limits;
struct RenderPassColorAttachment;
struct RequestAdapterOptions;
struct ShaderModuleDescriptor;
struct SurfaceDescriptor;
struct TexelCopyBufferInfo;
struct TexelCopyTextureInfo;
struct TextureComponentSwizzleDescriptor;
struct TextureDescriptor;
struct VertexBufferLayout;
struct BindGroupLayoutDescriptor;
struct ColorTargetState;
struct CompilationInfo;
struct ComputePipelineDescriptor;
struct DeviceDescriptor;
struct RenderPassDescriptor;
struct TextureViewDescriptor;
struct VertexState;
struct FragmentState;
struct RenderPipelineDescriptor;

// TODO(42241188): Remove once all clients use StringView versions of the callbacks.
// To make MSVC happy we need a StringView constructor from the adapter, so we first need to
// forward declare StringViewAdapter here. Otherwise MSVC complains about an ambiguous conversion.
namespace detail {
    struct StringViewAdapter;
}  // namespace detail

struct StringView {
    char const * data = nullptr;
    size_t length = WGPU_STRLEN;

    inline constexpr StringView() noexcept = default;

    // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
    inline constexpr StringView(const std::string_view& sv) noexcept {
        this->data = sv.data();
        this->length = sv.length();
    }

    // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
    inline constexpr StringView(const char* s) {
        this->data = s;
        this->length = WGPU_STRLEN;  // use strlen
    }

    // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
    inline constexpr StringView(WGPUStringView s) {
        this->data = s.data;
        this->length = s.length;
    }

    inline constexpr StringView(const char* data, size_t length) {
        this->data = data;
        this->length = length;
    }

    // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
    inline constexpr StringView(std::nullptr_t) {
        this->data = nullptr;
        this->length = WGPU_STRLEN;
    }

    // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
    inline constexpr StringView(std::nullopt_t) {
        this->data = nullptr;
        this->length = WGPU_STRLEN;
    }

    bool IsUndefined() const {
        return this->data == nullptr && this->length == wgpu::kStrlen;
    }

    // NOLINTNEXTLINE(runtime/explicit) allow implicit conversion
    operator std::string_view() const {
        if (this->length == wgpu::kStrlen) {
            if (IsUndefined()) {
                return {};
            }
            return {this->data};
        }
        return {this->data, this->length};
    }

    template <typename View,
              typename = std::enable_if_t<std::is_constructible_v<View, const char*, size_t>>>
    explicit operator View() const {
        if (this->length == wgpu::kStrlen) {
            if (IsUndefined()) {
                return {};
            }
            return {this->data};
        }
        return {this->data, this->length};
    }


    StringView(const detail::StringViewAdapter& s);
};

namespace detail {
constexpr size_t ConstexprMax(size_t a, size_t b) {
    return a > b ? a : b;
}

template <typename T>
static T& AsNonConstReference(const T& value) {
    return const_cast<T&>(value);
}

// A wrapper around StringView that can be implicitly converted to const char* with temporary
// storage that adds the \0 for output strings that are all explicitly-sized.
// TODO(42241188): Remove once all clients use StringView versions of the callbacks.
struct StringViewAdapter {
    WGPUStringView sv;
    char* nullTerminated = nullptr;

    StringViewAdapter(WGPUStringView sv) : sv(sv) {}
    ~StringViewAdapter() { delete[] nullTerminated; }
    operator ::WGPUStringView() { return sv; }
    operator StringView() { return {sv.data, sv.length}; }
    operator const char*() {
        assert(sv.length != WGPU_STRLEN);
        assert(nullTerminated == nullptr);
        nullTerminated = new char[sv.length + 1];
        for (size_t i = 0; i < sv.length; i++) {
            nullTerminated[i] = sv.data[i];
        }
        nullTerminated[sv.length] = 0;
        return nullTerminated;
    }
};
}  // namespace detail

inline StringView::StringView(const detail::StringViewAdapter& s): data(s.sv.data), length(s.sv.length) {}

namespace detail {
// For callbacks, we support two modes:
//   1) No userdata where we allow a std::function type that can include argument captures.
//   2) Explicit typed userdata where we only allow non-capturing lambdas or function pointers.
template <typename... Args>
struct CallbackTypeBase;
template <typename... Args>
struct CallbackTypeBase<std::tuple<Args...>> {
    using Callback = std::function<void(Args...)>;
};
template <typename... Args>
struct CallbackTypeBase<std::tuple<Args...>, void> {
    using Callback = void (Args...);
};
template <typename... Args, typename T>
struct CallbackTypeBase<std::tuple<Args...>, T> {
    using Callback = void (Args..., T);
};
}  // namespace detail


template <typename... T>
using BufferMapCallback = typename detail::CallbackTypeBase<std::tuple<MapAsyncStatus, StringView>, T...>::Callback;
template <typename... T>
using CompilationInfoCallback = typename detail::CallbackTypeBase<std::tuple<CompilationInfoRequestStatus, CompilationInfo const *>, T...>::Callback;
template <typename... T>
using CreateComputePipelineAsyncCallback = typename detail::CallbackTypeBase<std::tuple<CreatePipelineAsyncStatus, ComputePipeline, StringView>, T...>::Callback;
template <typename... T>
using CreateRenderPipelineAsyncCallback = typename detail::CallbackTypeBase<std::tuple<CreatePipelineAsyncStatus, RenderPipeline, StringView>, T...>::Callback;
template <typename... T>
using PopErrorScopeCallback = typename detail::CallbackTypeBase<std::tuple<PopErrorScopeStatus, ErrorType, StringView>, T...>::Callback;
template <typename... T>
using QueueWorkDoneCallback = typename detail::CallbackTypeBase<std::tuple<QueueWorkDoneStatus, StringView>, T...>::Callback;
template <typename... T>
using RequestAdapterCallback = typename detail::CallbackTypeBase<std::tuple<RequestAdapterStatus, Adapter, StringView>, T...>::Callback;
template <typename... T>
using RequestDeviceCallback = typename detail::CallbackTypeBase<std::tuple<RequestDeviceStatus, Device, StringView>, T...>::Callback;
template <typename... T>
using DeviceLostCallback = typename detail::CallbackTypeBase<std::tuple<const Device&, DeviceLostReason, StringView>, T...>::Callback;
template <typename... T>
using UncapturedErrorCallback = typename detail::CallbackTypeBase<std::tuple<const Device&, ErrorType, StringView>, T...>::Callback;




class Adapter : public ObjectBase<Adapter, WGPUAdapter> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    inline void GetFeatures(SupportedFeatures * features) const;
    inline ConvertibleStatus GetInfo(AdapterInfo * info) const;
    inline ConvertibleStatus GetLimits(Limits * limits) const;
    inline Bool HasFeature(FeatureName feature) const;
    template <typename F, typename T,
              typename Cb = RequestDeviceCallback<T>,
              typename CbChar = void (RequestDeviceStatus status, Device device, const char* message, T userdata),
              typename = std::enable_if_t<std::is_convertible_v<F, Cb*> || std::is_convertible_v<F, CbChar*>>>
    Future RequestDevice(DeviceDescriptor const * descriptor, CallbackMode callbackMode,F callback, T userdata) const;
    template <typename L,
              typename Cb = RequestDeviceCallback<>,
              typename CbChar = std::function<void(RequestDeviceStatus status, Device device, const char* message)>,
              typename = std::enable_if_t<std::is_convertible_v<L, Cb> || std::is_convertible_v<L, CbChar>>>
    Future RequestDevice(DeviceDescriptor const * descriptor, CallbackMode callbackMode,L callback) const;


  private:
    friend ObjectBase<Adapter, WGPUAdapter>;
    static inline void WGPUAddRef(WGPUAdapter handle);
    static inline void WGPURelease(WGPUAdapter handle);
};

class BindGroup : public ObjectBase<BindGroup, WGPUBindGroup> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    inline void SetLabel(StringView label) const;


  private:
    friend ObjectBase<BindGroup, WGPUBindGroup>;
    static inline void WGPUAddRef(WGPUBindGroup handle);
    static inline void WGPURelease(WGPUBindGroup handle);
};

class BindGroupLayout : public ObjectBase<BindGroupLayout, WGPUBindGroupLayout> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    inline void SetLabel(StringView label) const;


  private:
    friend ObjectBase<BindGroupLayout, WGPUBindGroupLayout>;
    static inline void WGPUAddRef(WGPUBindGroupLayout handle);
    static inline void WGPURelease(WGPUBindGroupLayout handle);
};

class Buffer : public ObjectBase<Buffer, WGPUBuffer> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    inline void Destroy() const;
    inline void const * GetConstMappedRange(size_t offset = 0, size_t size = kWholeMapSize) const;
    inline void * GetMappedRange(size_t offset = 0, size_t size = kWholeMapSize) const;
    inline BufferMapState GetMapState() const;
    inline uint64_t GetSize() const;
    inline BufferUsage GetUsage() const;
    template <typename F, typename T,
              typename Cb = BufferMapCallback<T>,
              typename CbChar = void (MapAsyncStatus status, const char* message, T userdata),
              typename = std::enable_if_t<std::is_convertible_v<F, Cb*> || std::is_convertible_v<F, CbChar*>>>
    Future MapAsync(MapMode mode, size_t offset, size_t size, CallbackMode callbackMode,F callback, T userdata) const;
    template <typename L,
              typename Cb = BufferMapCallback<>,
              typename CbChar = std::function<void(MapAsyncStatus status, const char* message)>,
              typename = std::enable_if_t<std::is_convertible_v<L, Cb> || std::is_convertible_v<L, CbChar>>>
    Future MapAsync(MapMode mode, size_t offset, size_t size, CallbackMode callbackMode,L callback) const;
    inline ConvertibleStatus ReadMappedRange(size_t offset, void * data, size_t size) const;
    inline void SetLabel(StringView label) const;
    inline void Unmap() const;
    inline ConvertibleStatus WriteMappedRange(size_t offset, void const * data, size_t size) const;


  private:
    friend ObjectBase<Buffer, WGPUBuffer>;
    static inline void WGPUAddRef(WGPUBuffer handle);
    static inline void WGPURelease(WGPUBuffer handle);
};

class CommandBuffer : public ObjectBase<CommandBuffer, WGPUCommandBuffer> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    inline void SetLabel(StringView label) const;


  private:
    friend ObjectBase<CommandBuffer, WGPUCommandBuffer>;
    static inline void WGPUAddRef(WGPUCommandBuffer handle);
    static inline void WGPURelease(WGPUCommandBuffer handle);
};

class CommandEncoder : public ObjectBase<CommandEncoder, WGPUCommandEncoder> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    inline ComputePassEncoder BeginComputePass(ComputePassDescriptor const * descriptor = nullptr) const;
    inline RenderPassEncoder BeginRenderPass(RenderPassDescriptor const * descriptor) const;
    inline void ClearBuffer(Buffer const& buffer, uint64_t offset = 0, uint64_t size = kWholeSize) const;
    inline void CopyBufferToBuffer(Buffer const& source, uint64_t sourceOffset, Buffer const& destination, uint64_t destinationOffset, uint64_t size) const;
    inline void CopyBufferToTexture(TexelCopyBufferInfo const * source, TexelCopyTextureInfo const * destination, Extent3D const * copySize) const;
    inline void CopyTextureToBuffer(TexelCopyTextureInfo const * source, TexelCopyBufferInfo const * destination, Extent3D const * copySize) const;
    inline void CopyTextureToTexture(TexelCopyTextureInfo const * source, TexelCopyTextureInfo const * destination, Extent3D const * copySize) const;
    inline CommandBuffer Finish(CommandBufferDescriptor const * descriptor = nullptr) const;
    inline void InsertDebugMarker(StringView markerLabel) const;
    inline void PopDebugGroup() const;
    inline void PushDebugGroup(StringView groupLabel) const;
    inline void ResolveQuerySet(QuerySet const& querySet, uint32_t firstQuery, uint32_t queryCount, Buffer const& destination, uint64_t destinationOffset) const;
    inline void SetLabel(StringView label) const;
    inline void WriteTimestamp(QuerySet const& querySet, uint32_t queryIndex) const;


  private:
    friend ObjectBase<CommandEncoder, WGPUCommandEncoder>;
    static inline void WGPUAddRef(WGPUCommandEncoder handle);
    static inline void WGPURelease(WGPUCommandEncoder handle);
};

class ComputePassEncoder : public ObjectBase<ComputePassEncoder, WGPUComputePassEncoder> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    inline void DispatchWorkgroups(uint32_t workgroupCountX, uint32_t workgroupCountY = 1, uint32_t workgroupCountZ = 1) const;
    inline void DispatchWorkgroupsIndirect(Buffer const& indirectBuffer, uint64_t indirectOffset) const;
    inline void End() const;
    inline void InsertDebugMarker(StringView markerLabel) const;
    inline void PopDebugGroup() const;
    inline void PushDebugGroup(StringView groupLabel) const;
    inline void SetBindGroup(uint32_t groupIndex, BindGroup const& group = nullptr, size_t dynamicOffsetCount = 0, uint32_t const * dynamicOffsets = nullptr) const;
    inline void SetLabel(StringView label) const;
    inline void SetPipeline(ComputePipeline const& pipeline) const;
    inline void WriteTimestamp(QuerySet const& querySet, uint32_t queryIndex) const;


  private:
    friend ObjectBase<ComputePassEncoder, WGPUComputePassEncoder>;
    static inline void WGPUAddRef(WGPUComputePassEncoder handle);
    static inline void WGPURelease(WGPUComputePassEncoder handle);
};

class ComputePipeline : public ObjectBase<ComputePipeline, WGPUComputePipeline> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    inline BindGroupLayout GetBindGroupLayout(uint32_t groupIndex) const;
    inline void SetLabel(StringView label) const;


  private:
    friend ObjectBase<ComputePipeline, WGPUComputePipeline>;
    static inline void WGPUAddRef(WGPUComputePipeline handle);
    static inline void WGPURelease(WGPUComputePipeline handle);
};

class Device : public ObjectBase<Device, WGPUDevice> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    inline BindGroup CreateBindGroup(BindGroupDescriptor const * descriptor) const;
    inline BindGroupLayout CreateBindGroupLayout(BindGroupLayoutDescriptor const * descriptor) const;
    inline Buffer CreateBuffer(BufferDescriptor const * descriptor) const;
    inline CommandEncoder CreateCommandEncoder(CommandEncoderDescriptor const * descriptor = nullptr) const;
    inline ComputePipeline CreateComputePipeline(ComputePipelineDescriptor const * descriptor) const;
    template <typename F, typename T,
              typename Cb = CreateComputePipelineAsyncCallback<T>,
              typename CbChar = void (CreatePipelineAsyncStatus status, ComputePipeline pipeline, const char* message, T userdata),
              typename = std::enable_if_t<std::is_convertible_v<F, Cb*> || std::is_convertible_v<F, CbChar*>>>
    Future CreateComputePipelineAsync(ComputePipelineDescriptor const * descriptor, CallbackMode callbackMode,F callback, T userdata) const;
    template <typename L,
              typename Cb = CreateComputePipelineAsyncCallback<>,
              typename CbChar = std::function<void(CreatePipelineAsyncStatus status, ComputePipeline pipeline, const char* message)>,
              typename = std::enable_if_t<std::is_convertible_v<L, Cb> || std::is_convertible_v<L, CbChar>>>
    Future CreateComputePipelineAsync(ComputePipelineDescriptor const * descriptor, CallbackMode callbackMode,L callback) const;
    inline PipelineLayout CreatePipelineLayout(PipelineLayoutDescriptor const * descriptor) const;
    inline QuerySet CreateQuerySet(QuerySetDescriptor const * descriptor) const;
    inline RenderBundleEncoder CreateRenderBundleEncoder(RenderBundleEncoderDescriptor const * descriptor) const;
    inline RenderPipeline CreateRenderPipeline(RenderPipelineDescriptor const * descriptor) const;
    template <typename F, typename T,
              typename Cb = CreateRenderPipelineAsyncCallback<T>,
              typename CbChar = void (CreatePipelineAsyncStatus status, RenderPipeline pipeline, const char* message, T userdata),
              typename = std::enable_if_t<std::is_convertible_v<F, Cb*> || std::is_convertible_v<F, CbChar*>>>
    Future CreateRenderPipelineAsync(RenderPipelineDescriptor const * descriptor, CallbackMode callbackMode,F callback, T userdata) const;
    template <typename L,
              typename Cb = CreateRenderPipelineAsyncCallback<>,
              typename CbChar = std::function<void(CreatePipelineAsyncStatus status, RenderPipeline pipeline, const char* message)>,
              typename = std::enable_if_t<std::is_convertible_v<L, Cb> || std::is_convertible_v<L, CbChar>>>
    Future CreateRenderPipelineAsync(RenderPipelineDescriptor const * descriptor, CallbackMode callbackMode,L callback) const;
    inline Sampler CreateSampler(SamplerDescriptor const * descriptor = nullptr) const;
    inline ShaderModule CreateShaderModule(ShaderModuleDescriptor const * descriptor) const;
    inline Texture CreateTexture(TextureDescriptor const * descriptor) const;
    inline void Destroy() const;
    inline ConvertibleStatus GetAdapterInfo(AdapterInfo * adapterInfo) const;
    inline void GetFeatures(SupportedFeatures * features) const;
    inline ConvertibleStatus GetLimits(Limits * limits) const;
    inline Future GetLostFuture() const;
    inline Queue GetQueue() const;
    inline Bool HasFeature(FeatureName feature) const;
    template <typename F, typename T,
              typename Cb = PopErrorScopeCallback<T>,
              typename CbChar = void (PopErrorScopeStatus status, ErrorType type, const char* message, T userdata),
              typename = std::enable_if_t<std::is_convertible_v<F, Cb*> || std::is_convertible_v<F, CbChar*>>>
    Future PopErrorScope(CallbackMode callbackMode,F callback, T userdata) const;
    template <typename L,
              typename Cb = PopErrorScopeCallback<>,
              typename CbChar = std::function<void(PopErrorScopeStatus status, ErrorType type, const char* message)>,
              typename = std::enable_if_t<std::is_convertible_v<L, Cb> || std::is_convertible_v<L, CbChar>>>
    Future PopErrorScope(CallbackMode callbackMode,L callback) const;
    inline void PushErrorScope(ErrorFilter filter) const;
    inline void SetLabel(StringView label) const;


  private:
    friend ObjectBase<Device, WGPUDevice>;
    static inline void WGPUAddRef(WGPUDevice handle);
    static inline void WGPURelease(WGPUDevice handle);
};

class Instance : public ObjectBase<Instance, WGPUInstance> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    inline Surface CreateSurface(SurfaceDescriptor const * descriptor) const;
    inline void GetWGSLLanguageFeatures(SupportedWGSLLanguageFeatures * features) const;
    inline Bool HasWGSLLanguageFeature(WGSLLanguageFeatureName feature) const;
    inline void ProcessEvents() const;
    template <typename F, typename T,
              typename Cb = RequestAdapterCallback<T>,
              typename CbChar = void (RequestAdapterStatus status, Adapter adapter, const char* message, T userdata),
              typename = std::enable_if_t<std::is_convertible_v<F, Cb*> || std::is_convertible_v<F, CbChar*>>>
    Future RequestAdapter(RequestAdapterOptions const * options, CallbackMode callbackMode,F callback, T userdata) const;
    template <typename L,
              typename Cb = RequestAdapterCallback<>,
              typename CbChar = std::function<void(RequestAdapterStatus status, Adapter adapter, const char* message)>,
              typename = std::enable_if_t<std::is_convertible_v<L, Cb> || std::is_convertible_v<L, CbChar>>>
    Future RequestAdapter(RequestAdapterOptions const * options, CallbackMode callbackMode,L callback) const;
    inline WaitStatus WaitAny(size_t futureCount, FutureWaitInfo * futures, uint64_t timeoutNS) const;

    inline WaitStatus WaitAny(Future f, uint64_t timeout) const;

  private:
    friend ObjectBase<Instance, WGPUInstance>;
    static inline void WGPUAddRef(WGPUInstance handle);
    static inline void WGPURelease(WGPUInstance handle);
};

class PipelineLayout : public ObjectBase<PipelineLayout, WGPUPipelineLayout> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    inline void SetLabel(StringView label) const;


  private:
    friend ObjectBase<PipelineLayout, WGPUPipelineLayout>;
    static inline void WGPUAddRef(WGPUPipelineLayout handle);
    static inline void WGPURelease(WGPUPipelineLayout handle);
};

class QuerySet : public ObjectBase<QuerySet, WGPUQuerySet> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    inline void Destroy() const;
    inline uint32_t GetCount() const;
    inline QueryType GetType() const;
    inline void SetLabel(StringView label) const;


  private:
    friend ObjectBase<QuerySet, WGPUQuerySet>;
    static inline void WGPUAddRef(WGPUQuerySet handle);
    static inline void WGPURelease(WGPUQuerySet handle);
};

class Queue : public ObjectBase<Queue, WGPUQueue> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    template <typename F, typename T,
              typename Cb = QueueWorkDoneCallback<T>,
              typename CbChar = void (QueueWorkDoneStatus status, const char* message, T userdata),
              typename = std::enable_if_t<std::is_convertible_v<F, Cb*> || std::is_convertible_v<F, CbChar*>>>
    Future OnSubmittedWorkDone(CallbackMode callbackMode,F callback, T userdata) const;
    template <typename L,
              typename Cb = QueueWorkDoneCallback<>,
              typename CbChar = std::function<void(QueueWorkDoneStatus status, const char* message)>,
              typename = std::enable_if_t<std::is_convertible_v<L, Cb> || std::is_convertible_v<L, CbChar>>>
    Future OnSubmittedWorkDone(CallbackMode callbackMode,L callback) const;
    inline void SetLabel(StringView label) const;
    inline void Submit(size_t commandCount, CommandBuffer const * commands) const;
    inline void WriteBuffer(Buffer const& buffer, uint64_t bufferOffset, void const * data, size_t size) const;
    inline void WriteTexture(TexelCopyTextureInfo const * destination, void const * data, size_t dataSize, TexelCopyBufferLayout const * dataLayout, Extent3D const * writeSize) const;


  private:
    friend ObjectBase<Queue, WGPUQueue>;
    static inline void WGPUAddRef(WGPUQueue handle);
    static inline void WGPURelease(WGPUQueue handle);
};

class RenderBundle : public ObjectBase<RenderBundle, WGPURenderBundle> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    inline void SetLabel(StringView label) const;


  private:
    friend ObjectBase<RenderBundle, WGPURenderBundle>;
    static inline void WGPUAddRef(WGPURenderBundle handle);
    static inline void WGPURelease(WGPURenderBundle handle);
};

class RenderBundleEncoder : public ObjectBase<RenderBundleEncoder, WGPURenderBundleEncoder> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    inline void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0) const;
    inline void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t baseVertex = 0, uint32_t firstInstance = 0) const;
    inline void DrawIndexedIndirect(Buffer const& indirectBuffer, uint64_t indirectOffset) const;
    inline void DrawIndirect(Buffer const& indirectBuffer, uint64_t indirectOffset) const;
    inline RenderBundle Finish(RenderBundleDescriptor const * descriptor = nullptr) const;
    inline void InsertDebugMarker(StringView markerLabel) const;
    inline void PopDebugGroup() const;
    inline void PushDebugGroup(StringView groupLabel) const;
    inline void SetBindGroup(uint32_t groupIndex, BindGroup const& group = nullptr, size_t dynamicOffsetCount = 0, uint32_t const * dynamicOffsets = nullptr) const;
    inline void SetIndexBuffer(Buffer const& buffer, IndexFormat format, uint64_t offset = 0, uint64_t size = kWholeSize) const;
    inline void SetLabel(StringView label) const;
    inline void SetPipeline(RenderPipeline const& pipeline) const;
    inline void SetVertexBuffer(uint32_t slot, Buffer const& buffer = nullptr, uint64_t offset = 0, uint64_t size = kWholeSize) const;


  private:
    friend ObjectBase<RenderBundleEncoder, WGPURenderBundleEncoder>;
    static inline void WGPUAddRef(WGPURenderBundleEncoder handle);
    static inline void WGPURelease(WGPURenderBundleEncoder handle);
};

class RenderPassEncoder : public ObjectBase<RenderPassEncoder, WGPURenderPassEncoder> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    inline void BeginOcclusionQuery(uint32_t queryIndex) const;
    inline void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0) const;
    inline void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t baseVertex = 0, uint32_t firstInstance = 0) const;
    inline void DrawIndexedIndirect(Buffer const& indirectBuffer, uint64_t indirectOffset) const;
    inline void DrawIndirect(Buffer const& indirectBuffer, uint64_t indirectOffset) const;
    inline void End() const;
    inline void EndOcclusionQuery() const;
    inline void ExecuteBundles(size_t bundleCount, RenderBundle const * bundles) const;
    inline void InsertDebugMarker(StringView markerLabel) const;
    inline void MultiDrawIndexedIndirect(Buffer const& indirectBuffer, uint64_t indirectOffset, uint32_t maxDrawCount, Buffer const& drawCountBuffer = nullptr, uint64_t drawCountBufferOffset = 0) const;
    inline void MultiDrawIndirect(Buffer const& indirectBuffer, uint64_t indirectOffset, uint32_t maxDrawCount, Buffer const& drawCountBuffer = nullptr, uint64_t drawCountBufferOffset = 0) const;
    inline void PopDebugGroup() const;
    inline void PushDebugGroup(StringView groupLabel) const;
    inline void SetBindGroup(uint32_t groupIndex, BindGroup const& group = nullptr, size_t dynamicOffsetCount = 0, uint32_t const * dynamicOffsets = nullptr) const;
    inline void SetBlendConstant(Color const * color) const;
    inline void SetIndexBuffer(Buffer const& buffer, IndexFormat format, uint64_t offset = 0, uint64_t size = kWholeSize) const;
    inline void SetLabel(StringView label) const;
    inline void SetPipeline(RenderPipeline const& pipeline) const;
    inline void SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) const;
    inline void SetStencilReference(uint32_t reference) const;
    inline void SetVertexBuffer(uint32_t slot, Buffer const& buffer = nullptr, uint64_t offset = 0, uint64_t size = kWholeSize) const;
    inline void SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth) const;
    inline void WriteTimestamp(QuerySet const& querySet, uint32_t queryIndex) const;


  private:
    friend ObjectBase<RenderPassEncoder, WGPURenderPassEncoder>;
    static inline void WGPUAddRef(WGPURenderPassEncoder handle);
    static inline void WGPURelease(WGPURenderPassEncoder handle);
};

class RenderPipeline : public ObjectBase<RenderPipeline, WGPURenderPipeline> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    inline BindGroupLayout GetBindGroupLayout(uint32_t groupIndex) const;
    inline void SetLabel(StringView label) const;


  private:
    friend ObjectBase<RenderPipeline, WGPURenderPipeline>;
    static inline void WGPUAddRef(WGPURenderPipeline handle);
    static inline void WGPURelease(WGPURenderPipeline handle);
};

class Sampler : public ObjectBase<Sampler, WGPUSampler> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    inline void SetLabel(StringView label) const;


  private:
    friend ObjectBase<Sampler, WGPUSampler>;
    static inline void WGPUAddRef(WGPUSampler handle);
    static inline void WGPURelease(WGPUSampler handle);
};

class ShaderModule : public ObjectBase<ShaderModule, WGPUShaderModule> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    template <typename F, typename T,
              typename Cb = CompilationInfoCallback<T>,
              typename CbChar = void (CompilationInfoRequestStatus status, CompilationInfo const * compilationInfo, T userdata),
              typename = std::enable_if_t<std::is_convertible_v<F, Cb*> || std::is_convertible_v<F, CbChar*>>>
    Future GetCompilationInfo(CallbackMode callbackMode,F callback, T userdata) const;
    template <typename L,
              typename Cb = CompilationInfoCallback<>,
              typename CbChar = std::function<void(CompilationInfoRequestStatus status, CompilationInfo const * compilationInfo)>,
              typename = std::enable_if_t<std::is_convertible_v<L, Cb> || std::is_convertible_v<L, CbChar>>>
    Future GetCompilationInfo(CallbackMode callbackMode,L callback) const;
    inline void SetLabel(StringView label) const;


  private:
    friend ObjectBase<ShaderModule, WGPUShaderModule>;
    static inline void WGPUAddRef(WGPUShaderModule handle);
    static inline void WGPURelease(WGPUShaderModule handle);
};

class Surface : public ObjectBase<Surface, WGPUSurface> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    inline void Configure(SurfaceConfiguration const * config) const;
    inline ConvertibleStatus GetCapabilities(Adapter const& adapter, SurfaceCapabilities * capabilities) const;
    inline void GetCurrentTexture(SurfaceTexture * surfaceTexture) const;
    inline ConvertibleStatus Present() const;
    inline void SetLabel(StringView label) const;
    inline void Unconfigure() const;


  private:
    friend ObjectBase<Surface, WGPUSurface>;
    static inline void WGPUAddRef(WGPUSurface handle);
    static inline void WGPURelease(WGPUSurface handle);
};

class Texture : public ObjectBase<Texture, WGPUTexture> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    inline TextureView CreateView(TextureViewDescriptor const * descriptor = nullptr) const;
    inline void Destroy() const;
    inline uint32_t GetDepthOrArrayLayers() const;
    inline TextureDimension GetDimension() const;
    inline TextureFormat GetFormat() const;
    inline uint32_t GetHeight() const;
    inline uint32_t GetMipLevelCount() const;
    inline uint32_t GetSampleCount() const;
    inline TextureUsage GetUsage() const;
    inline uint32_t GetWidth() const;
    inline void SetLabel(StringView label) const;


  private:
    friend ObjectBase<Texture, WGPUTexture>;
    static inline void WGPUAddRef(WGPUTexture handle);
    static inline void WGPURelease(WGPUTexture handle);
};

class TextureView : public ObjectBase<TextureView, WGPUTextureView> {
  public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

    inline void SetLabel(StringView label) const;


  private:
    friend ObjectBase<TextureView, WGPUTextureView>;
    static inline void WGPUAddRef(WGPUTextureView handle);
    static inline void WGPURelease(WGPUTextureView handle);
};


// ChainedStruct
static_assert(sizeof(ChainedStruct) == sizeof(WGPUChainedStruct),
    "sizeof mismatch for ChainedStruct");
static_assert(alignof(ChainedStruct) == alignof(WGPUChainedStruct),
    "alignof mismatch for ChainedStruct");
static_assert(offsetof(ChainedStruct, nextInChain) == offsetof(WGPUChainedStruct, next),
    "offsetof mismatch for ChainedStruct::nextInChain");
static_assert(offsetof(ChainedStruct, sType) == offsetof(WGPUChainedStruct, sType),
    "offsetof mismatch for ChainedStruct::sType");


struct AdapterInfo {
    inline AdapterInfo();
    inline ~AdapterInfo();
    AdapterInfo(const AdapterInfo&) = delete;
    AdapterInfo& operator=(const AdapterInfo&) = delete;
    inline AdapterInfo(AdapterInfo&&);
    inline AdapterInfo& operator=(AdapterInfo&&);
    inline operator const WGPUAdapterInfo&() const noexcept;

    ChainedStructOut  * nextInChain = nullptr;
    StringView const vendor = {};
    StringView const architecture = {};
    StringView const device = {};
    StringView const description = {};
    BackendType const backendType = BackendType::Undefined;
    AdapterType const adapterType = {};
    uint32_t const vendorID = {};
    uint32_t const deviceID = {};
    uint32_t const subgroupMinSize = {};
    uint32_t const subgroupMaxSize = {};

  private:
    inline void FreeMembers();
    static inline void Reset(AdapterInfo& value);
};

struct BindGroupEntry {
    inline operator const WGPUBindGroupEntry&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    uint32_t binding;
    Buffer buffer = nullptr;
    uint64_t offset = 0;
    uint64_t size = kWholeSize;
    Sampler sampler = nullptr;
    TextureView textureView = nullptr;
};

struct BlendComponent {
    inline operator const WGPUBlendComponent&() const noexcept;

    BlendOperation operation = BlendOperation::Undefined;
    BlendFactor srcFactor = BlendFactor::Undefined;
    BlendFactor dstFactor = BlendFactor::Undefined;
};

struct BufferBindingLayout {
    inline operator const WGPUBufferBindingLayout&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    BufferBindingType type = BufferBindingType::Undefined;
    Bool hasDynamicOffset = false;
    uint64_t minBindingSize = 0;
};

struct BufferDescriptor {
    inline operator const WGPUBufferDescriptor&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView label = {};
    BufferUsage usage = BufferUsage::None;
    uint64_t size;
    Bool mappedAtCreation = false;
};

struct Color {
    inline operator const WGPUColor&() const noexcept;

    double r;
    double g;
    double b;
    double a;
};

struct CommandBufferDescriptor {
    inline operator const WGPUCommandBufferDescriptor&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView label = {};
};

struct CommandEncoderDescriptor {
    inline operator const WGPUCommandEncoderDescriptor&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView label = {};
};

// Can be chained in Limits
struct CompatibilityModeLimits : ChainedStructOut {
    inline CompatibilityModeLimits();

    struct Init;
    inline CompatibilityModeLimits(Init&& init);
    inline operator const WGPUCompatibilityModeLimits&() const noexcept;

    static constexpr size_t kFirstMemberAlignment = detail::ConstexprMax(alignof(ChainedStruct), alignof(uint32_t));
    alignas(kFirstMemberAlignment) uint32_t maxStorageBuffersInVertexStage = kLimitU32Undefined;
    uint32_t maxStorageTexturesInVertexStage = kLimitU32Undefined;
    uint32_t maxStorageBuffersInFragmentStage = kLimitU32Undefined;
    uint32_t maxStorageTexturesInFragmentStage = kLimitU32Undefined;
};

struct ConstantEntry {
    inline operator const WGPUConstantEntry&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView key = {};
    double value;
};

// Can be chained in CompilationMessage
struct DawnCompilationMessageUtf16 : ChainedStruct {
    inline DawnCompilationMessageUtf16();

    struct Init;
    inline DawnCompilationMessageUtf16(Init&& init);
    inline operator const WGPUDawnCompilationMessageUtf16&() const noexcept;

    static constexpr size_t kFirstMemberAlignment = detail::ConstexprMax(alignof(ChainedStruct), alignof(uint64_t));
    alignas(kFirstMemberAlignment) uint64_t linePos;
    uint64_t offset;
    uint64_t length;
};

// Can be chained in SurfaceDescriptor
struct EmscriptenSurfaceSourceCanvasHTMLSelector : ChainedStruct {
    inline EmscriptenSurfaceSourceCanvasHTMLSelector();

    struct Init;
    inline EmscriptenSurfaceSourceCanvasHTMLSelector(Init&& init);
    inline operator const WGPUEmscriptenSurfaceSourceCanvasHTMLSelector&() const noexcept;

    static constexpr size_t kFirstMemberAlignment = detail::ConstexprMax(alignof(ChainedStruct), alignof(StringView));
    alignas(kFirstMemberAlignment) StringView selector = {};
};

struct Extent3D {
    inline operator const WGPUExtent3D&() const noexcept;

    uint32_t width;
    uint32_t height = 1;
    uint32_t depthOrArrayLayers = 1;
};

struct Future {
    inline operator const WGPUFuture&() const noexcept;

    uint64_t id;
};

struct InstanceLimits {
    inline operator const WGPUInstanceLimits&() const noexcept;

    ChainedStructOut  * nextInChain = nullptr;
    size_t timedWaitAnyMaxCount = 0;
};

struct INTERNAL_HAVE_EMDAWNWEBGPU_HEADER {
    inline operator const WGPUINTERNAL_HAVE_EMDAWNWEBGPU_HEADER&() const noexcept;

    Bool unused = false;
};

struct MultisampleState {
    inline operator const WGPUMultisampleState&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    uint32_t count = 1;
    uint32_t mask = 0xFFFFFFFF;
    Bool alphaToCoverageEnabled = false;
};

struct Origin3D {
    inline operator const WGPUOrigin3D&() const noexcept;

    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t z = 0;
};

struct PassTimestampWrites {
    inline operator const WGPUPassTimestampWrites&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    QuerySet querySet = nullptr;
    uint32_t beginningOfPassWriteIndex = kQuerySetIndexUndefined;
    uint32_t endOfPassWriteIndex = kQuerySetIndexUndefined;
};

struct PipelineLayoutDescriptor {
    inline operator const WGPUPipelineLayoutDescriptor&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView label = {};
    size_t bindGroupLayoutCount;
    BindGroupLayout const * bindGroupLayouts = nullptr;
    uint32_t immediateSize = 0;
};

struct PrimitiveState {
    inline operator const WGPUPrimitiveState&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    PrimitiveTopology topology = PrimitiveTopology::Undefined;
    IndexFormat stripIndexFormat = IndexFormat::Undefined;
    FrontFace frontFace = FrontFace::Undefined;
    CullMode cullMode = CullMode::Undefined;
    Bool unclippedDepth = false;
};

struct QuerySetDescriptor {
    inline operator const WGPUQuerySetDescriptor&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView label = {};
    QueryType type = {};
    uint32_t count;
};

struct QueueDescriptor {
    inline operator const WGPUQueueDescriptor&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView label = {};
};

struct RenderBundleDescriptor {
    inline operator const WGPURenderBundleDescriptor&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView label = {};
};

struct RenderBundleEncoderDescriptor {
    inline operator const WGPURenderBundleEncoderDescriptor&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView label = {};
    size_t colorFormatCount;
    TextureFormat const * colorFormats = nullptr;
    TextureFormat depthStencilFormat = TextureFormat::Undefined;
    uint32_t sampleCount = 1;
    Bool depthReadOnly = false;
    Bool stencilReadOnly = false;
};

struct RenderPassDepthStencilAttachment {
    inline operator const WGPURenderPassDepthStencilAttachment&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    TextureView view = nullptr;
    LoadOp depthLoadOp = LoadOp::Undefined;
    StoreOp depthStoreOp = StoreOp::Undefined;
    float depthClearValue = kDepthClearValueUndefined;
    Bool depthReadOnly = false;
    LoadOp stencilLoadOp = LoadOp::Undefined;
    StoreOp stencilStoreOp = StoreOp::Undefined;
    uint32_t stencilClearValue = 0;
    Bool stencilReadOnly = false;
};

// Can be chained in RenderPassDescriptor
struct RenderPassMaxDrawCount : ChainedStruct {
    inline RenderPassMaxDrawCount();

    struct Init;
    inline RenderPassMaxDrawCount(Init&& init);
    inline operator const WGPURenderPassMaxDrawCount&() const noexcept;

    static constexpr size_t kFirstMemberAlignment = detail::ConstexprMax(alignof(ChainedStruct), alignof(uint64_t));
    alignas(kFirstMemberAlignment) uint64_t maxDrawCount = 50000000;
};

// Can be chained in RequestAdapterOptions
struct RequestAdapterWebXROptions : ChainedStruct {
    inline RequestAdapterWebXROptions();

    struct Init;
    inline RequestAdapterWebXROptions(Init&& init);
    inline operator const WGPURequestAdapterWebXROptions&() const noexcept;

    static constexpr size_t kFirstMemberAlignment = detail::ConstexprMax(alignof(ChainedStruct), alignof(Bool));
    alignas(kFirstMemberAlignment) Bool xrCompatible;
};

struct SamplerBindingLayout {
    inline operator const WGPUSamplerBindingLayout&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    SamplerBindingType type = SamplerBindingType::Undefined;
};

struct SamplerDescriptor {
    inline operator const WGPUSamplerDescriptor&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView label = {};
    AddressMode addressModeU = AddressMode::Undefined;
    AddressMode addressModeV = AddressMode::Undefined;
    AddressMode addressModeW = AddressMode::Undefined;
    FilterMode magFilter = FilterMode::Undefined;
    FilterMode minFilter = FilterMode::Undefined;
    MipmapFilterMode mipmapFilter = MipmapFilterMode::Undefined;
    float lodMinClamp = 0.f;
    float lodMaxClamp = 32.f;
    CompareFunction compare = CompareFunction::Undefined;
    uint16_t maxAnisotropy = 1;
};

// Can be chained in ShaderModuleDescriptor
struct ShaderSourceSPIRV : ChainedStruct {
    inline ShaderSourceSPIRV();

    struct Init;
    inline ShaderSourceSPIRV(Init&& init);
    inline operator const WGPUShaderSourceSPIRV&() const noexcept;

    static constexpr size_t kFirstMemberAlignment = detail::ConstexprMax(alignof(ChainedStruct), alignof(uint32_t));
    alignas(kFirstMemberAlignment) uint32_t codeSize;
    uint32_t const * code = nullptr;
};

// Can be chained in ShaderModuleDescriptor
struct ShaderSourceWGSL : ChainedStruct {
    inline ShaderSourceWGSL();

    struct Init;
    inline ShaderSourceWGSL(Init&& init);
    inline operator const WGPUShaderSourceWGSL&() const noexcept;

    static constexpr size_t kFirstMemberAlignment = detail::ConstexprMax(alignof(ChainedStruct), alignof(StringView));
    alignas(kFirstMemberAlignment) StringView code = {};
};

struct StencilFaceState {
    inline operator const WGPUStencilFaceState&() const noexcept;

    CompareFunction compare = CompareFunction::Undefined;
    StencilOperation failOp = StencilOperation::Undefined;
    StencilOperation depthFailOp = StencilOperation::Undefined;
    StencilOperation passOp = StencilOperation::Undefined;
};

struct StorageTextureBindingLayout {
    inline operator const WGPUStorageTextureBindingLayout&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StorageTextureAccess access = StorageTextureAccess::Undefined;
    TextureFormat format = TextureFormat::Undefined;
    TextureViewDimension viewDimension = TextureViewDimension::Undefined;
};

struct SupportedFeatures {
    inline SupportedFeatures();
    inline ~SupportedFeatures();
    SupportedFeatures(const SupportedFeatures&) = delete;
    SupportedFeatures& operator=(const SupportedFeatures&) = delete;
    inline SupportedFeatures(SupportedFeatures&&);
    inline SupportedFeatures& operator=(SupportedFeatures&&);
    inline operator const WGPUSupportedFeatures&() const noexcept;

    size_t const featureCount = {};
    FeatureName const * const features = nullptr;

  private:
    inline void FreeMembers();
    static inline void Reset(SupportedFeatures& value);
};

struct SupportedInstanceFeatures {
    inline SupportedInstanceFeatures();
    inline ~SupportedInstanceFeatures();
    SupportedInstanceFeatures(const SupportedInstanceFeatures&) = delete;
    SupportedInstanceFeatures& operator=(const SupportedInstanceFeatures&) = delete;
    inline SupportedInstanceFeatures(SupportedInstanceFeatures&&);
    inline SupportedInstanceFeatures& operator=(SupportedInstanceFeatures&&);
    inline operator const WGPUSupportedInstanceFeatures&() const noexcept;

    size_t const featureCount = {};
    InstanceFeatureName const * const features = nullptr;

  private:
    inline void FreeMembers();
    static inline void Reset(SupportedInstanceFeatures& value);
};

struct SupportedWGSLLanguageFeatures {
    inline SupportedWGSLLanguageFeatures();
    inline ~SupportedWGSLLanguageFeatures();
    SupportedWGSLLanguageFeatures(const SupportedWGSLLanguageFeatures&) = delete;
    SupportedWGSLLanguageFeatures& operator=(const SupportedWGSLLanguageFeatures&) = delete;
    inline SupportedWGSLLanguageFeatures(SupportedWGSLLanguageFeatures&&);
    inline SupportedWGSLLanguageFeatures& operator=(SupportedWGSLLanguageFeatures&&);
    inline operator const WGPUSupportedWGSLLanguageFeatures&() const noexcept;

    size_t const featureCount = {};
    WGSLLanguageFeatureName const * const features = nullptr;

  private:
    inline void FreeMembers();
    static inline void Reset(SupportedWGSLLanguageFeatures& value);
};

struct SurfaceCapabilities {
    inline SurfaceCapabilities();
    inline ~SurfaceCapabilities();
    SurfaceCapabilities(const SurfaceCapabilities&) = delete;
    SurfaceCapabilities& operator=(const SurfaceCapabilities&) = delete;
    inline SurfaceCapabilities(SurfaceCapabilities&&);
    inline SurfaceCapabilities& operator=(SurfaceCapabilities&&);
    inline operator const WGPUSurfaceCapabilities&() const noexcept;

    ChainedStructOut  * nextInChain = nullptr;
    TextureUsage const usages = TextureUsage::None;
    size_t const formatCount = {};
    TextureFormat const * const formats = nullptr;
    size_t const presentModeCount = {};
    PresentMode const * const presentModes = nullptr;
    size_t const alphaModeCount = {};
    CompositeAlphaMode const * const alphaModes = nullptr;

  private:
    inline void FreeMembers();
    static inline void Reset(SurfaceCapabilities& value);
};

// Can be chained in SurfaceDescriptor
struct SurfaceColorManagement : ChainedStruct {
    inline SurfaceColorManagement();

    struct Init;
    inline SurfaceColorManagement(Init&& init);
    inline operator const WGPUSurfaceColorManagement&() const noexcept;

    static constexpr size_t kFirstMemberAlignment = detail::ConstexprMax(alignof(ChainedStruct), alignof(PredefinedColorSpace));
    alignas(kFirstMemberAlignment) PredefinedColorSpace colorSpace = {};
    ToneMappingMode toneMappingMode = {};
};

struct SurfaceConfiguration {
    inline operator const WGPUSurfaceConfiguration&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    Device device = nullptr;
    TextureFormat format = TextureFormat::Undefined;
    TextureUsage usage = TextureUsage::RenderAttachment;
    uint32_t width;
    uint32_t height;
    size_t viewFormatCount = 0;
    TextureFormat const * viewFormats = nullptr;
    CompositeAlphaMode alphaMode = CompositeAlphaMode::Auto;
    PresentMode presentMode = PresentMode::Undefined;
};

struct SurfaceTexture {
    inline operator const WGPUSurfaceTexture&() const noexcept;

    ChainedStructOut  * nextInChain = nullptr;
    Texture texture = nullptr;
    SurfaceGetCurrentTextureStatus status = {};
};

struct TexelCopyBufferLayout {
    inline operator const WGPUTexelCopyBufferLayout&() const noexcept;

    uint64_t offset = 0;
    uint32_t bytesPerRow = kCopyStrideUndefined;
    uint32_t rowsPerImage = kCopyStrideUndefined;
};

struct TextureBindingLayout {
    inline operator const WGPUTextureBindingLayout&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    TextureSampleType sampleType = TextureSampleType::Undefined;
    TextureViewDimension viewDimension = TextureViewDimension::Undefined;
    Bool multisampled = false;
};

// Can be chained in TextureDescriptor
struct TextureBindingViewDimensionDescriptor : ChainedStruct {
    inline TextureBindingViewDimensionDescriptor();

    struct Init;
    inline TextureBindingViewDimensionDescriptor(Init&& init);
    inline operator const WGPUTextureBindingViewDimensionDescriptor&() const noexcept;

    static constexpr size_t kFirstMemberAlignment = detail::ConstexprMax(alignof(ChainedStruct), alignof(TextureViewDimension));
    alignas(kFirstMemberAlignment) TextureViewDimension textureBindingViewDimension = TextureViewDimension::Undefined;
};

struct TextureComponentSwizzle {
    inline operator const WGPUTextureComponentSwizzle&() const noexcept;

    ComponentSwizzle r = ComponentSwizzle::Undefined;
    ComponentSwizzle g = ComponentSwizzle::Undefined;
    ComponentSwizzle b = ComponentSwizzle::Undefined;
    ComponentSwizzle a = ComponentSwizzle::Undefined;
};

struct VertexAttribute {
    inline operator const WGPUVertexAttribute&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    VertexFormat format = {};
    uint64_t offset;
    uint32_t shaderLocation;
};

struct BindGroupDescriptor {
    inline operator const WGPUBindGroupDescriptor&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView label = {};
    BindGroupLayout layout = nullptr;
    size_t entryCount = 0;
    BindGroupEntry const * entries = nullptr;
};

struct BindGroupLayoutEntry {
    inline operator const WGPUBindGroupLayoutEntry&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    uint32_t binding;
    ShaderStage visibility = ShaderStage::None;
    uint32_t bindingArraySize = 0;
    BufferBindingLayout buffer = { nullptr, BufferBindingType::BindingNotUsed, false, 0 };
    SamplerBindingLayout sampler = { nullptr, SamplerBindingType::BindingNotUsed };
    TextureBindingLayout texture = { nullptr, TextureSampleType::BindingNotUsed, TextureViewDimension::e2D, false };
    StorageTextureBindingLayout storageTexture = { nullptr, StorageTextureAccess::BindingNotUsed, TextureFormat::Undefined, TextureViewDimension::e2D };
};

struct BlendState {
    inline operator const WGPUBlendState&() const noexcept;

    BlendComponent color = {};
    BlendComponent alpha = {};
};

struct CompilationMessage {
    inline operator const WGPUCompilationMessage&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView message = {};
    CompilationMessageType type = {};
    uint64_t lineNum;
    uint64_t linePos;
    uint64_t offset;
    uint64_t length;
};

struct ComputePassDescriptor {
    inline operator const WGPUComputePassDescriptor&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView label = {};
    PassTimestampWrites const * timestampWrites = nullptr;
};

struct ComputeState {
    inline operator const WGPUComputeState&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    ShaderModule module = nullptr;
    StringView entryPoint = {};
    size_t constantCount = 0;
    ConstantEntry const * constants = nullptr;
};

struct DepthStencilState {
    inline operator const WGPUDepthStencilState&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    TextureFormat format = TextureFormat::Undefined;
    OptionalBool depthWriteEnabled = OptionalBool::Undefined;
    CompareFunction depthCompare = CompareFunction::Undefined;
    StencilFaceState stencilFront = {};
    StencilFaceState stencilBack = {};
    uint32_t stencilReadMask = 0xFFFFFFFF;
    uint32_t stencilWriteMask = 0xFFFFFFFF;
    int32_t depthBias = 0;
    float depthBiasSlopeScale = 0.f;
    float depthBiasClamp = 0.f;
};

struct FutureWaitInfo {
    inline operator const WGPUFutureWaitInfo&() const noexcept;

    Future future = {};
    Bool completed = false;
};

struct InstanceDescriptor {
    inline operator const WGPUInstanceDescriptor&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    size_t requiredFeatureCount = 0;
    InstanceFeatureName const * requiredFeatures = nullptr;
    InstanceLimits const * requiredLimits = nullptr;
};

struct Limits {
    inline operator const WGPULimits&() const noexcept;

    ChainedStructOut  * nextInChain = nullptr;
    uint32_t maxTextureDimension1D = kLimitU32Undefined;
    uint32_t maxTextureDimension2D = kLimitU32Undefined;
    uint32_t maxTextureDimension3D = kLimitU32Undefined;
    uint32_t maxTextureArrayLayers = kLimitU32Undefined;
    uint32_t maxBindGroups = kLimitU32Undefined;
    uint32_t maxBindGroupsPlusVertexBuffers = kLimitU32Undefined;
    uint32_t maxBindingsPerBindGroup = kLimitU32Undefined;
    uint32_t maxDynamicUniformBuffersPerPipelineLayout = kLimitU32Undefined;
    uint32_t maxDynamicStorageBuffersPerPipelineLayout = kLimitU32Undefined;
    uint32_t maxSampledTexturesPerShaderStage = kLimitU32Undefined;
    uint32_t maxSamplersPerShaderStage = kLimitU32Undefined;
    uint32_t maxStorageBuffersPerShaderStage = kLimitU32Undefined;
    uint32_t maxStorageTexturesPerShaderStage = kLimitU32Undefined;
    uint32_t maxUniformBuffersPerShaderStage = kLimitU32Undefined;
    uint64_t maxUniformBufferBindingSize = kLimitU64Undefined;
    uint64_t maxStorageBufferBindingSize = kLimitU64Undefined;
    uint32_t minUniformBufferOffsetAlignment = kLimitU32Undefined;
    uint32_t minStorageBufferOffsetAlignment = kLimitU32Undefined;
    uint32_t maxVertexBuffers = kLimitU32Undefined;
    uint64_t maxBufferSize = kLimitU64Undefined;
    uint32_t maxVertexAttributes = kLimitU32Undefined;
    uint32_t maxVertexBufferArrayStride = kLimitU32Undefined;
    uint32_t maxInterStageShaderVariables = kLimitU32Undefined;
    uint32_t maxColorAttachments = kLimitU32Undefined;
    uint32_t maxColorAttachmentBytesPerSample = kLimitU32Undefined;
    uint32_t maxComputeWorkgroupStorageSize = kLimitU32Undefined;
    uint32_t maxComputeInvocationsPerWorkgroup = kLimitU32Undefined;
    uint32_t maxComputeWorkgroupSizeX = kLimitU32Undefined;
    uint32_t maxComputeWorkgroupSizeY = kLimitU32Undefined;
    uint32_t maxComputeWorkgroupSizeZ = kLimitU32Undefined;
    uint32_t maxComputeWorkgroupsPerDimension = kLimitU32Undefined;
    uint32_t maxImmediateSize = kLimitU32Undefined;
};

struct RenderPassColorAttachment {
    inline operator const WGPURenderPassColorAttachment&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    TextureView view = nullptr;
    uint32_t depthSlice = kDepthSliceUndefined;
    TextureView resolveTarget = nullptr;
    LoadOp loadOp = LoadOp::Undefined;
    StoreOp storeOp = StoreOp::Undefined;
    Color clearValue = {};
};

struct RequestAdapterOptions {
    inline operator const WGPURequestAdapterOptions&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    FeatureLevel featureLevel = FeatureLevel::Undefined;
    PowerPreference powerPreference = PowerPreference::Undefined;
    Bool forceFallbackAdapter = false;
    BackendType backendType = BackendType::Undefined;
    Surface compatibleSurface = nullptr;
};

struct ShaderModuleDescriptor {
    inline operator const WGPUShaderModuleDescriptor&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView label = {};
};

struct SurfaceDescriptor {
    inline operator const WGPUSurfaceDescriptor&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView label = {};
};

struct TexelCopyBufferInfo {
    inline operator const WGPUTexelCopyBufferInfo&() const noexcept;

    TexelCopyBufferLayout layout = {};
    Buffer buffer = nullptr;
};

struct TexelCopyTextureInfo {
    inline operator const WGPUTexelCopyTextureInfo&() const noexcept;

    Texture texture = nullptr;
    uint32_t mipLevel = 0;
    Origin3D origin = {};
    TextureAspect aspect = TextureAspect::Undefined;
};

// Can be chained in TextureViewDescriptor
struct TextureComponentSwizzleDescriptor : ChainedStruct {
    inline TextureComponentSwizzleDescriptor();

    struct Init;
    inline TextureComponentSwizzleDescriptor(Init&& init);
    inline operator const WGPUTextureComponentSwizzleDescriptor&() const noexcept;

    static constexpr size_t kFirstMemberAlignment = detail::ConstexprMax(alignof(ChainedStruct), alignof(TextureComponentSwizzle));
    alignas(kFirstMemberAlignment) TextureComponentSwizzle swizzle = {};
};

struct TextureDescriptor {
    inline operator const WGPUTextureDescriptor&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView label = {};
    TextureUsage usage = TextureUsage::None;
    TextureDimension dimension = TextureDimension::Undefined;
    Extent3D size = {};
    TextureFormat format = TextureFormat::Undefined;
    uint32_t mipLevelCount = 1;
    uint32_t sampleCount = 1;
    size_t viewFormatCount = 0;
    TextureFormat const * viewFormats = nullptr;
};

struct VertexBufferLayout {
    inline operator const WGPUVertexBufferLayout&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    VertexStepMode stepMode = VertexStepMode::Undefined;
    uint64_t arrayStride;
    size_t attributeCount;
    VertexAttribute const * attributes = nullptr;
};

struct BindGroupLayoutDescriptor {
    inline operator const WGPUBindGroupLayoutDescriptor&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView label = {};
    size_t entryCount = 0;
    BindGroupLayoutEntry const * entries = nullptr;
};

struct ColorTargetState {
    inline operator const WGPUColorTargetState&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    TextureFormat format = TextureFormat::Undefined;
    BlendState const * blend = nullptr;
    ColorWriteMask writeMask = ColorWriteMask::All;
};

struct CompilationInfo {
    inline operator const WGPUCompilationInfo&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    size_t messageCount;
    CompilationMessage const * messages = nullptr;
};

struct ComputePipelineDescriptor {
    inline operator const WGPUComputePipelineDescriptor&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView label = {};
    PipelineLayout layout = nullptr;
    ComputeState compute = {};
};

struct RenderPassDescriptor {
    inline operator const WGPURenderPassDescriptor&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView label = {};
    size_t colorAttachmentCount;
    RenderPassColorAttachment const * colorAttachments = nullptr;
    RenderPassDepthStencilAttachment const * depthStencilAttachment = nullptr;
    QuerySet occlusionQuerySet = nullptr;
    PassTimestampWrites const * timestampWrites = nullptr;
};

struct TextureViewDescriptor {
    inline operator const WGPUTextureViewDescriptor&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView label = {};
    TextureFormat format = TextureFormat::Undefined;
    TextureViewDimension dimension = TextureViewDimension::Undefined;
    uint32_t baseMipLevel = 0;
    uint32_t mipLevelCount = kMipLevelCountUndefined;
    uint32_t baseArrayLayer = 0;
    uint32_t arrayLayerCount = kArrayLayerCountUndefined;
    TextureAspect aspect = TextureAspect::Undefined;
    TextureUsage usage = TextureUsage::None;
};

struct VertexState {
    inline operator const WGPUVertexState&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    ShaderModule module = nullptr;
    StringView entryPoint = {};
    size_t constantCount = 0;
    ConstantEntry const * constants = nullptr;
    size_t bufferCount = 0;
    VertexBufferLayout const * buffers = nullptr;
};

struct FragmentState {
    inline operator const WGPUFragmentState&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    ShaderModule module = nullptr;
    StringView entryPoint = {};
    size_t constantCount = 0;
    ConstantEntry const * constants = nullptr;
    size_t targetCount;
    ColorTargetState const * targets = nullptr;
};

struct RenderPipelineDescriptor {
    inline operator const WGPURenderPipelineDescriptor&() const noexcept;

    ChainedStruct const * nextInChain = nullptr;
    StringView label = {};
    PipelineLayout layout = nullptr;
    VertexState vertex = {};
    PrimitiveState primitive = {};
    DepthStencilState const * depthStencil = nullptr;
    MultisampleState multisample = {};
    FragmentState const * fragment = nullptr;
};


namespace detail {
struct DeviceDescriptor {
    ChainedStruct const * nextInChain = nullptr;
    StringView label = {};
    size_t requiredFeatureCount = 0;
    FeatureName const * requiredFeatures = nullptr;
    Limits const * requiredLimits = nullptr;
    QueueDescriptor defaultQueue = {};
    WGPUDeviceLostCallbackInfo deviceLostCallbackInfo = WGPU_DEVICE_LOST_CALLBACK_INFO_INIT;
    WGPUUncapturedErrorCallbackInfo uncapturedErrorCallbackInfo = WGPU_UNCAPTURED_ERROR_CALLBACK_INFO_INIT;
};
}  // namespace detail
struct DeviceDescriptor : protected detail::DeviceDescriptor {
    inline operator const WGPUDeviceDescriptor&() const noexcept;

    using detail::DeviceDescriptor::nextInChain;
    using detail::DeviceDescriptor::label;
    using detail::DeviceDescriptor::requiredFeatureCount;
    using detail::DeviceDescriptor::requiredFeatures;
    using detail::DeviceDescriptor::requiredLimits;
    using detail::DeviceDescriptor::defaultQueue;

    inline DeviceDescriptor();
    struct Init;
    inline DeviceDescriptor(Init&& init);

    template <typename F, typename T,
              typename Cb = DeviceLostCallback<T>,
              typename = std::enable_if_t<std::is_convertible_v<F, Cb*>>>
    void SetDeviceLostCallback(CallbackMode callbackMode, F callback, T userdata);
    template <typename L,
              typename Cb = DeviceLostCallback<>,
              typename = std::enable_if_t<std::is_convertible_v<L, Cb>>>
    void SetDeviceLostCallback(CallbackMode callbackMode, L callback);

    template <typename F, typename T,
              typename Cb = UncapturedErrorCallback<T>,
              typename = std::enable_if_t<std::is_convertible_v<F, Cb*>>>
    void SetUncapturedErrorCallback(F callback, T userdata);
    template <typename L,
              typename Cb = UncapturedErrorCallback<>,
              typename = std::enable_if_t<std::is_convertible_v<L, Cb>>>
    void SetUncapturedErrorCallback(L callback);
};

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
// error: 'offsetof' within non-standard-layout type 'wgpu::XXX' is conditionally-supported
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

// AdapterInfo implementation
AdapterInfo::AdapterInfo() = default;
AdapterInfo::~AdapterInfo() {
    FreeMembers();
}

AdapterInfo::AdapterInfo(AdapterInfo&& rhs)
    : vendor(rhs.vendor),
            architecture(rhs.architecture),
            device(rhs.device),
            description(rhs.description),
            backendType(rhs.backendType),
            adapterType(rhs.adapterType),
            vendorID(rhs.vendorID),
            deviceID(rhs.deviceID),
            subgroupMinSize(rhs.subgroupMinSize),
            subgroupMaxSize(rhs.subgroupMaxSize){
    Reset(rhs);
}

AdapterInfo& AdapterInfo::operator=(AdapterInfo&& rhs) {
    if (&rhs == this) {
        return *this;
    }
    FreeMembers();
    detail::AsNonConstReference(this->vendor) = std::move(rhs.vendor);
    detail::AsNonConstReference(this->architecture) = std::move(rhs.architecture);
    detail::AsNonConstReference(this->device) = std::move(rhs.device);
    detail::AsNonConstReference(this->description) = std::move(rhs.description);
    detail::AsNonConstReference(this->backendType) = std::move(rhs.backendType);
    detail::AsNonConstReference(this->adapterType) = std::move(rhs.adapterType);
    detail::AsNonConstReference(this->vendorID) = std::move(rhs.vendorID);
    detail::AsNonConstReference(this->deviceID) = std::move(rhs.deviceID);
    detail::AsNonConstReference(this->subgroupMinSize) = std::move(rhs.subgroupMinSize);
    detail::AsNonConstReference(this->subgroupMaxSize) = std::move(rhs.subgroupMaxSize);
    Reset(rhs);
    return *this;
}

void AdapterInfo::FreeMembers() {
    bool needsFreeing = false;    if (this->vendor.data != nullptr) { needsFreeing = true; }    if (this->architecture.data != nullptr) { needsFreeing = true; }    if (this->device.data != nullptr) { needsFreeing = true; }    if (this->description.data != nullptr) { needsFreeing = true; }if (needsFreeing) {
        wgpuAdapterInfoFreeMembers(
            *reinterpret_cast<WGPUAdapterInfo*>(this));
    }
}

// static
void AdapterInfo::Reset(AdapterInfo& value) {
    AdapterInfo defaultValue{};
    detail::AsNonConstReference(value.vendor) = defaultValue.vendor;
    detail::AsNonConstReference(value.architecture) = defaultValue.architecture;
    detail::AsNonConstReference(value.device) = defaultValue.device;
    detail::AsNonConstReference(value.description) = defaultValue.description;
    detail::AsNonConstReference(value.backendType) = defaultValue.backendType;
    detail::AsNonConstReference(value.adapterType) = defaultValue.adapterType;
    detail::AsNonConstReference(value.vendorID) = defaultValue.vendorID;
    detail::AsNonConstReference(value.deviceID) = defaultValue.deviceID;
    detail::AsNonConstReference(value.subgroupMinSize) = defaultValue.subgroupMinSize;
    detail::AsNonConstReference(value.subgroupMaxSize) = defaultValue.subgroupMaxSize;
}

AdapterInfo::operator const WGPUAdapterInfo&() const noexcept {
    return *reinterpret_cast<const WGPUAdapterInfo*>(this);
}

static_assert(sizeof(AdapterInfo) == sizeof(WGPUAdapterInfo), "sizeof mismatch for AdapterInfo");
static_assert(alignof(AdapterInfo) == alignof(WGPUAdapterInfo), "alignof mismatch for AdapterInfo");
static_assert(offsetof(AdapterInfo, nextInChain) == offsetof(WGPUAdapterInfo, nextInChain),
        "offsetof mismatch for AdapterInfo::nextInChain");
static_assert(offsetof(AdapterInfo, vendor) == offsetof(WGPUAdapterInfo, vendor),
        "offsetof mismatch for AdapterInfo::vendor");
static_assert(offsetof(AdapterInfo, architecture) == offsetof(WGPUAdapterInfo, architecture),
        "offsetof mismatch for AdapterInfo::architecture");
static_assert(offsetof(AdapterInfo, device) == offsetof(WGPUAdapterInfo, device),
        "offsetof mismatch for AdapterInfo::device");
static_assert(offsetof(AdapterInfo, description) == offsetof(WGPUAdapterInfo, description),
        "offsetof mismatch for AdapterInfo::description");
static_assert(offsetof(AdapterInfo, backendType) == offsetof(WGPUAdapterInfo, backendType),
        "offsetof mismatch for AdapterInfo::backendType");
static_assert(offsetof(AdapterInfo, adapterType) == offsetof(WGPUAdapterInfo, adapterType),
        "offsetof mismatch for AdapterInfo::adapterType");
static_assert(offsetof(AdapterInfo, vendorID) == offsetof(WGPUAdapterInfo, vendorID),
        "offsetof mismatch for AdapterInfo::vendorID");
static_assert(offsetof(AdapterInfo, deviceID) == offsetof(WGPUAdapterInfo, deviceID),
        "offsetof mismatch for AdapterInfo::deviceID");
static_assert(offsetof(AdapterInfo, subgroupMinSize) == offsetof(WGPUAdapterInfo, subgroupMinSize),
        "offsetof mismatch for AdapterInfo::subgroupMinSize");
static_assert(offsetof(AdapterInfo, subgroupMaxSize) == offsetof(WGPUAdapterInfo, subgroupMaxSize),
        "offsetof mismatch for AdapterInfo::subgroupMaxSize");

// BindGroupEntry implementation

BindGroupEntry::operator const WGPUBindGroupEntry&() const noexcept {
    return *reinterpret_cast<const WGPUBindGroupEntry*>(this);
}

static_assert(sizeof(BindGroupEntry) == sizeof(WGPUBindGroupEntry), "sizeof mismatch for BindGroupEntry");
static_assert(alignof(BindGroupEntry) == alignof(WGPUBindGroupEntry), "alignof mismatch for BindGroupEntry");
static_assert(offsetof(BindGroupEntry, nextInChain) == offsetof(WGPUBindGroupEntry, nextInChain),
        "offsetof mismatch for BindGroupEntry::nextInChain");
static_assert(offsetof(BindGroupEntry, binding) == offsetof(WGPUBindGroupEntry, binding),
        "offsetof mismatch for BindGroupEntry::binding");
static_assert(offsetof(BindGroupEntry, buffer) == offsetof(WGPUBindGroupEntry, buffer),
        "offsetof mismatch for BindGroupEntry::buffer");
static_assert(offsetof(BindGroupEntry, offset) == offsetof(WGPUBindGroupEntry, offset),
        "offsetof mismatch for BindGroupEntry::offset");
static_assert(offsetof(BindGroupEntry, size) == offsetof(WGPUBindGroupEntry, size),
        "offsetof mismatch for BindGroupEntry::size");
static_assert(offsetof(BindGroupEntry, sampler) == offsetof(WGPUBindGroupEntry, sampler),
        "offsetof mismatch for BindGroupEntry::sampler");
static_assert(offsetof(BindGroupEntry, textureView) == offsetof(WGPUBindGroupEntry, textureView),
        "offsetof mismatch for BindGroupEntry::textureView");

// BlendComponent implementation

BlendComponent::operator const WGPUBlendComponent&() const noexcept {
    return *reinterpret_cast<const WGPUBlendComponent*>(this);
}

static_assert(sizeof(BlendComponent) == sizeof(WGPUBlendComponent), "sizeof mismatch for BlendComponent");
static_assert(alignof(BlendComponent) == alignof(WGPUBlendComponent), "alignof mismatch for BlendComponent");
static_assert(offsetof(BlendComponent, operation) == offsetof(WGPUBlendComponent, operation),
        "offsetof mismatch for BlendComponent::operation");
static_assert(offsetof(BlendComponent, srcFactor) == offsetof(WGPUBlendComponent, srcFactor),
        "offsetof mismatch for BlendComponent::srcFactor");
static_assert(offsetof(BlendComponent, dstFactor) == offsetof(WGPUBlendComponent, dstFactor),
        "offsetof mismatch for BlendComponent::dstFactor");

// BufferBindingLayout implementation

BufferBindingLayout::operator const WGPUBufferBindingLayout&() const noexcept {
    return *reinterpret_cast<const WGPUBufferBindingLayout*>(this);
}

static_assert(sizeof(BufferBindingLayout) == sizeof(WGPUBufferBindingLayout), "sizeof mismatch for BufferBindingLayout");
static_assert(alignof(BufferBindingLayout) == alignof(WGPUBufferBindingLayout), "alignof mismatch for BufferBindingLayout");
static_assert(offsetof(BufferBindingLayout, nextInChain) == offsetof(WGPUBufferBindingLayout, nextInChain),
        "offsetof mismatch for BufferBindingLayout::nextInChain");
static_assert(offsetof(BufferBindingLayout, type) == offsetof(WGPUBufferBindingLayout, type),
        "offsetof mismatch for BufferBindingLayout::type");
static_assert(offsetof(BufferBindingLayout, hasDynamicOffset) == offsetof(WGPUBufferBindingLayout, hasDynamicOffset),
        "offsetof mismatch for BufferBindingLayout::hasDynamicOffset");
static_assert(offsetof(BufferBindingLayout, minBindingSize) == offsetof(WGPUBufferBindingLayout, minBindingSize),
        "offsetof mismatch for BufferBindingLayout::minBindingSize");

// BufferDescriptor implementation

BufferDescriptor::operator const WGPUBufferDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPUBufferDescriptor*>(this);
}

static_assert(sizeof(BufferDescriptor) == sizeof(WGPUBufferDescriptor), "sizeof mismatch for BufferDescriptor");
static_assert(alignof(BufferDescriptor) == alignof(WGPUBufferDescriptor), "alignof mismatch for BufferDescriptor");
static_assert(offsetof(BufferDescriptor, nextInChain) == offsetof(WGPUBufferDescriptor, nextInChain),
        "offsetof mismatch for BufferDescriptor::nextInChain");
static_assert(offsetof(BufferDescriptor, label) == offsetof(WGPUBufferDescriptor, label),
        "offsetof mismatch for BufferDescriptor::label");
static_assert(offsetof(BufferDescriptor, usage) == offsetof(WGPUBufferDescriptor, usage),
        "offsetof mismatch for BufferDescriptor::usage");
static_assert(offsetof(BufferDescriptor, size) == offsetof(WGPUBufferDescriptor, size),
        "offsetof mismatch for BufferDescriptor::size");
static_assert(offsetof(BufferDescriptor, mappedAtCreation) == offsetof(WGPUBufferDescriptor, mappedAtCreation),
        "offsetof mismatch for BufferDescriptor::mappedAtCreation");

// Color implementation

Color::operator const WGPUColor&() const noexcept {
    return *reinterpret_cast<const WGPUColor*>(this);
}

static_assert(sizeof(Color) == sizeof(WGPUColor), "sizeof mismatch for Color");
static_assert(alignof(Color) == alignof(WGPUColor), "alignof mismatch for Color");
static_assert(offsetof(Color, r) == offsetof(WGPUColor, r),
        "offsetof mismatch for Color::r");
static_assert(offsetof(Color, g) == offsetof(WGPUColor, g),
        "offsetof mismatch for Color::g");
static_assert(offsetof(Color, b) == offsetof(WGPUColor, b),
        "offsetof mismatch for Color::b");
static_assert(offsetof(Color, a) == offsetof(WGPUColor, a),
        "offsetof mismatch for Color::a");

// CommandBufferDescriptor implementation

CommandBufferDescriptor::operator const WGPUCommandBufferDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPUCommandBufferDescriptor*>(this);
}

static_assert(sizeof(CommandBufferDescriptor) == sizeof(WGPUCommandBufferDescriptor), "sizeof mismatch for CommandBufferDescriptor");
static_assert(alignof(CommandBufferDescriptor) == alignof(WGPUCommandBufferDescriptor), "alignof mismatch for CommandBufferDescriptor");
static_assert(offsetof(CommandBufferDescriptor, nextInChain) == offsetof(WGPUCommandBufferDescriptor, nextInChain),
        "offsetof mismatch for CommandBufferDescriptor::nextInChain");
static_assert(offsetof(CommandBufferDescriptor, label) == offsetof(WGPUCommandBufferDescriptor, label),
        "offsetof mismatch for CommandBufferDescriptor::label");

// CommandEncoderDescriptor implementation

CommandEncoderDescriptor::operator const WGPUCommandEncoderDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPUCommandEncoderDescriptor*>(this);
}

static_assert(sizeof(CommandEncoderDescriptor) == sizeof(WGPUCommandEncoderDescriptor), "sizeof mismatch for CommandEncoderDescriptor");
static_assert(alignof(CommandEncoderDescriptor) == alignof(WGPUCommandEncoderDescriptor), "alignof mismatch for CommandEncoderDescriptor");
static_assert(offsetof(CommandEncoderDescriptor, nextInChain) == offsetof(WGPUCommandEncoderDescriptor, nextInChain),
        "offsetof mismatch for CommandEncoderDescriptor::nextInChain");
static_assert(offsetof(CommandEncoderDescriptor, label) == offsetof(WGPUCommandEncoderDescriptor, label),
        "offsetof mismatch for CommandEncoderDescriptor::label");

// CompatibilityModeLimits implementation
CompatibilityModeLimits::CompatibilityModeLimits()
  : ChainedStructOut { nullptr, SType::CompatibilityModeLimits } {}
struct CompatibilityModeLimits::Init {
    ChainedStructOut *  nextInChain;
    uint32_t maxStorageBuffersInVertexStage = kLimitU32Undefined;
    uint32_t maxStorageTexturesInVertexStage = kLimitU32Undefined;
    uint32_t maxStorageBuffersInFragmentStage = kLimitU32Undefined;
    uint32_t maxStorageTexturesInFragmentStage = kLimitU32Undefined;
};
CompatibilityModeLimits::CompatibilityModeLimits(CompatibilityModeLimits::Init&& init)
  : ChainedStructOut { init.nextInChain, SType::CompatibilityModeLimits }, 
    maxStorageBuffersInVertexStage(std::move(init.maxStorageBuffersInVertexStage)), 
    maxStorageTexturesInVertexStage(std::move(init.maxStorageTexturesInVertexStage)), 
    maxStorageBuffersInFragmentStage(std::move(init.maxStorageBuffersInFragmentStage)), 
    maxStorageTexturesInFragmentStage(std::move(init.maxStorageTexturesInFragmentStage)){}

CompatibilityModeLimits::operator const WGPUCompatibilityModeLimits&() const noexcept {
    return *reinterpret_cast<const WGPUCompatibilityModeLimits*>(this);
}

static_assert(sizeof(CompatibilityModeLimits) == sizeof(WGPUCompatibilityModeLimits), "sizeof mismatch for CompatibilityModeLimits");
static_assert(alignof(CompatibilityModeLimits) == alignof(WGPUCompatibilityModeLimits), "alignof mismatch for CompatibilityModeLimits");
static_assert(offsetof(CompatibilityModeLimits, maxStorageBuffersInVertexStage) == offsetof(WGPUCompatibilityModeLimits, maxStorageBuffersInVertexStage),
        "offsetof mismatch for CompatibilityModeLimits::maxStorageBuffersInVertexStage");
static_assert(offsetof(CompatibilityModeLimits, maxStorageTexturesInVertexStage) == offsetof(WGPUCompatibilityModeLimits, maxStorageTexturesInVertexStage),
        "offsetof mismatch for CompatibilityModeLimits::maxStorageTexturesInVertexStage");
static_assert(offsetof(CompatibilityModeLimits, maxStorageBuffersInFragmentStage) == offsetof(WGPUCompatibilityModeLimits, maxStorageBuffersInFragmentStage),
        "offsetof mismatch for CompatibilityModeLimits::maxStorageBuffersInFragmentStage");
static_assert(offsetof(CompatibilityModeLimits, maxStorageTexturesInFragmentStage) == offsetof(WGPUCompatibilityModeLimits, maxStorageTexturesInFragmentStage),
        "offsetof mismatch for CompatibilityModeLimits::maxStorageTexturesInFragmentStage");

// ConstantEntry implementation

ConstantEntry::operator const WGPUConstantEntry&() const noexcept {
    return *reinterpret_cast<const WGPUConstantEntry*>(this);
}

static_assert(sizeof(ConstantEntry) == sizeof(WGPUConstantEntry), "sizeof mismatch for ConstantEntry");
static_assert(alignof(ConstantEntry) == alignof(WGPUConstantEntry), "alignof mismatch for ConstantEntry");
static_assert(offsetof(ConstantEntry, nextInChain) == offsetof(WGPUConstantEntry, nextInChain),
        "offsetof mismatch for ConstantEntry::nextInChain");
static_assert(offsetof(ConstantEntry, key) == offsetof(WGPUConstantEntry, key),
        "offsetof mismatch for ConstantEntry::key");
static_assert(offsetof(ConstantEntry, value) == offsetof(WGPUConstantEntry, value),
        "offsetof mismatch for ConstantEntry::value");

// DawnCompilationMessageUtf16 implementation
DawnCompilationMessageUtf16::DawnCompilationMessageUtf16()
  : ChainedStruct { nullptr, SType::DawnCompilationMessageUtf16 } {}
struct DawnCompilationMessageUtf16::Init {
    ChainedStruct * const nextInChain;
    uint64_t linePos;
    uint64_t offset;
    uint64_t length;
};
DawnCompilationMessageUtf16::DawnCompilationMessageUtf16(DawnCompilationMessageUtf16::Init&& init)
  : ChainedStruct { init.nextInChain, SType::DawnCompilationMessageUtf16 }, 
    linePos(std::move(init.linePos)), 
    offset(std::move(init.offset)), 
    length(std::move(init.length)){}

DawnCompilationMessageUtf16::operator const WGPUDawnCompilationMessageUtf16&() const noexcept {
    return *reinterpret_cast<const WGPUDawnCompilationMessageUtf16*>(this);
}

static_assert(sizeof(DawnCompilationMessageUtf16) == sizeof(WGPUDawnCompilationMessageUtf16), "sizeof mismatch for DawnCompilationMessageUtf16");
static_assert(alignof(DawnCompilationMessageUtf16) == alignof(WGPUDawnCompilationMessageUtf16), "alignof mismatch for DawnCompilationMessageUtf16");
static_assert(offsetof(DawnCompilationMessageUtf16, linePos) == offsetof(WGPUDawnCompilationMessageUtf16, linePos),
        "offsetof mismatch for DawnCompilationMessageUtf16::linePos");
static_assert(offsetof(DawnCompilationMessageUtf16, offset) == offsetof(WGPUDawnCompilationMessageUtf16, offset),
        "offsetof mismatch for DawnCompilationMessageUtf16::offset");
static_assert(offsetof(DawnCompilationMessageUtf16, length) == offsetof(WGPUDawnCompilationMessageUtf16, length),
        "offsetof mismatch for DawnCompilationMessageUtf16::length");

// EmscriptenSurfaceSourceCanvasHTMLSelector implementation
EmscriptenSurfaceSourceCanvasHTMLSelector::EmscriptenSurfaceSourceCanvasHTMLSelector()
  : ChainedStruct { nullptr, SType::EmscriptenSurfaceSourceCanvasHTMLSelector } {}
struct EmscriptenSurfaceSourceCanvasHTMLSelector::Init {
    ChainedStruct * const nextInChain;
    StringView selector = {};
};
EmscriptenSurfaceSourceCanvasHTMLSelector::EmscriptenSurfaceSourceCanvasHTMLSelector(EmscriptenSurfaceSourceCanvasHTMLSelector::Init&& init)
  : ChainedStruct { init.nextInChain, SType::EmscriptenSurfaceSourceCanvasHTMLSelector }, 
    selector(std::move(init.selector)){}

EmscriptenSurfaceSourceCanvasHTMLSelector::operator const WGPUEmscriptenSurfaceSourceCanvasHTMLSelector&() const noexcept {
    return *reinterpret_cast<const WGPUEmscriptenSurfaceSourceCanvasHTMLSelector*>(this);
}

static_assert(sizeof(EmscriptenSurfaceSourceCanvasHTMLSelector) == sizeof(WGPUEmscriptenSurfaceSourceCanvasHTMLSelector), "sizeof mismatch for EmscriptenSurfaceSourceCanvasHTMLSelector");
static_assert(alignof(EmscriptenSurfaceSourceCanvasHTMLSelector) == alignof(WGPUEmscriptenSurfaceSourceCanvasHTMLSelector), "alignof mismatch for EmscriptenSurfaceSourceCanvasHTMLSelector");
static_assert(offsetof(EmscriptenSurfaceSourceCanvasHTMLSelector, selector) == offsetof(WGPUEmscriptenSurfaceSourceCanvasHTMLSelector, selector),
        "offsetof mismatch for EmscriptenSurfaceSourceCanvasHTMLSelector::selector");

// Extent3D implementation

Extent3D::operator const WGPUExtent3D&() const noexcept {
    return *reinterpret_cast<const WGPUExtent3D*>(this);
}

static_assert(sizeof(Extent3D) == sizeof(WGPUExtent3D), "sizeof mismatch for Extent3D");
static_assert(alignof(Extent3D) == alignof(WGPUExtent3D), "alignof mismatch for Extent3D");
static_assert(offsetof(Extent3D, width) == offsetof(WGPUExtent3D, width),
        "offsetof mismatch for Extent3D::width");
static_assert(offsetof(Extent3D, height) == offsetof(WGPUExtent3D, height),
        "offsetof mismatch for Extent3D::height");
static_assert(offsetof(Extent3D, depthOrArrayLayers) == offsetof(WGPUExtent3D, depthOrArrayLayers),
        "offsetof mismatch for Extent3D::depthOrArrayLayers");

// Future implementation

Future::operator const WGPUFuture&() const noexcept {
    return *reinterpret_cast<const WGPUFuture*>(this);
}

static_assert(sizeof(Future) == sizeof(WGPUFuture), "sizeof mismatch for Future");
static_assert(alignof(Future) == alignof(WGPUFuture), "alignof mismatch for Future");
static_assert(offsetof(Future, id) == offsetof(WGPUFuture, id),
        "offsetof mismatch for Future::id");

// InstanceLimits implementation

InstanceLimits::operator const WGPUInstanceLimits&() const noexcept {
    return *reinterpret_cast<const WGPUInstanceLimits*>(this);
}

static_assert(sizeof(InstanceLimits) == sizeof(WGPUInstanceLimits), "sizeof mismatch for InstanceLimits");
static_assert(alignof(InstanceLimits) == alignof(WGPUInstanceLimits), "alignof mismatch for InstanceLimits");
static_assert(offsetof(InstanceLimits, nextInChain) == offsetof(WGPUInstanceLimits, nextInChain),
        "offsetof mismatch for InstanceLimits::nextInChain");
static_assert(offsetof(InstanceLimits, timedWaitAnyMaxCount) == offsetof(WGPUInstanceLimits, timedWaitAnyMaxCount),
        "offsetof mismatch for InstanceLimits::timedWaitAnyMaxCount");

// INTERNAL_HAVE_EMDAWNWEBGPU_HEADER implementation

INTERNAL_HAVE_EMDAWNWEBGPU_HEADER::operator const WGPUINTERNAL_HAVE_EMDAWNWEBGPU_HEADER&() const noexcept {
    return *reinterpret_cast<const WGPUINTERNAL_HAVE_EMDAWNWEBGPU_HEADER*>(this);
}

static_assert(sizeof(INTERNAL_HAVE_EMDAWNWEBGPU_HEADER) == sizeof(WGPUINTERNAL_HAVE_EMDAWNWEBGPU_HEADER), "sizeof mismatch for INTERNAL_HAVE_EMDAWNWEBGPU_HEADER");
static_assert(alignof(INTERNAL_HAVE_EMDAWNWEBGPU_HEADER) == alignof(WGPUINTERNAL_HAVE_EMDAWNWEBGPU_HEADER), "alignof mismatch for INTERNAL_HAVE_EMDAWNWEBGPU_HEADER");
static_assert(offsetof(INTERNAL_HAVE_EMDAWNWEBGPU_HEADER, unused) == offsetof(WGPUINTERNAL_HAVE_EMDAWNWEBGPU_HEADER, unused),
        "offsetof mismatch for INTERNAL_HAVE_EMDAWNWEBGPU_HEADER::unused");

// MultisampleState implementation

MultisampleState::operator const WGPUMultisampleState&() const noexcept {
    return *reinterpret_cast<const WGPUMultisampleState*>(this);
}

static_assert(sizeof(MultisampleState) == sizeof(WGPUMultisampleState), "sizeof mismatch for MultisampleState");
static_assert(alignof(MultisampleState) == alignof(WGPUMultisampleState), "alignof mismatch for MultisampleState");
static_assert(offsetof(MultisampleState, nextInChain) == offsetof(WGPUMultisampleState, nextInChain),
        "offsetof mismatch for MultisampleState::nextInChain");
static_assert(offsetof(MultisampleState, count) == offsetof(WGPUMultisampleState, count),
        "offsetof mismatch for MultisampleState::count");
static_assert(offsetof(MultisampleState, mask) == offsetof(WGPUMultisampleState, mask),
        "offsetof mismatch for MultisampleState::mask");
static_assert(offsetof(MultisampleState, alphaToCoverageEnabled) == offsetof(WGPUMultisampleState, alphaToCoverageEnabled),
        "offsetof mismatch for MultisampleState::alphaToCoverageEnabled");

// Origin3D implementation

Origin3D::operator const WGPUOrigin3D&() const noexcept {
    return *reinterpret_cast<const WGPUOrigin3D*>(this);
}

static_assert(sizeof(Origin3D) == sizeof(WGPUOrigin3D), "sizeof mismatch for Origin3D");
static_assert(alignof(Origin3D) == alignof(WGPUOrigin3D), "alignof mismatch for Origin3D");
static_assert(offsetof(Origin3D, x) == offsetof(WGPUOrigin3D, x),
        "offsetof mismatch for Origin3D::x");
static_assert(offsetof(Origin3D, y) == offsetof(WGPUOrigin3D, y),
        "offsetof mismatch for Origin3D::y");
static_assert(offsetof(Origin3D, z) == offsetof(WGPUOrigin3D, z),
        "offsetof mismatch for Origin3D::z");

// PassTimestampWrites implementation

PassTimestampWrites::operator const WGPUPassTimestampWrites&() const noexcept {
    return *reinterpret_cast<const WGPUPassTimestampWrites*>(this);
}

static_assert(sizeof(PassTimestampWrites) == sizeof(WGPUPassTimestampWrites), "sizeof mismatch for PassTimestampWrites");
static_assert(alignof(PassTimestampWrites) == alignof(WGPUPassTimestampWrites), "alignof mismatch for PassTimestampWrites");
static_assert(offsetof(PassTimestampWrites, nextInChain) == offsetof(WGPUPassTimestampWrites, nextInChain),
        "offsetof mismatch for PassTimestampWrites::nextInChain");
static_assert(offsetof(PassTimestampWrites, querySet) == offsetof(WGPUPassTimestampWrites, querySet),
        "offsetof mismatch for PassTimestampWrites::querySet");
static_assert(offsetof(PassTimestampWrites, beginningOfPassWriteIndex) == offsetof(WGPUPassTimestampWrites, beginningOfPassWriteIndex),
        "offsetof mismatch for PassTimestampWrites::beginningOfPassWriteIndex");
static_assert(offsetof(PassTimestampWrites, endOfPassWriteIndex) == offsetof(WGPUPassTimestampWrites, endOfPassWriteIndex),
        "offsetof mismatch for PassTimestampWrites::endOfPassWriteIndex");

// PipelineLayoutDescriptor implementation

PipelineLayoutDescriptor::operator const WGPUPipelineLayoutDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPUPipelineLayoutDescriptor*>(this);
}

static_assert(sizeof(PipelineLayoutDescriptor) == sizeof(WGPUPipelineLayoutDescriptor), "sizeof mismatch for PipelineLayoutDescriptor");
static_assert(alignof(PipelineLayoutDescriptor) == alignof(WGPUPipelineLayoutDescriptor), "alignof mismatch for PipelineLayoutDescriptor");
static_assert(offsetof(PipelineLayoutDescriptor, nextInChain) == offsetof(WGPUPipelineLayoutDescriptor, nextInChain),
        "offsetof mismatch for PipelineLayoutDescriptor::nextInChain");
static_assert(offsetof(PipelineLayoutDescriptor, label) == offsetof(WGPUPipelineLayoutDescriptor, label),
        "offsetof mismatch for PipelineLayoutDescriptor::label");
static_assert(offsetof(PipelineLayoutDescriptor, bindGroupLayoutCount) == offsetof(WGPUPipelineLayoutDescriptor, bindGroupLayoutCount),
        "offsetof mismatch for PipelineLayoutDescriptor::bindGroupLayoutCount");
static_assert(offsetof(PipelineLayoutDescriptor, bindGroupLayouts) == offsetof(WGPUPipelineLayoutDescriptor, bindGroupLayouts),
        "offsetof mismatch for PipelineLayoutDescriptor::bindGroupLayouts");
static_assert(offsetof(PipelineLayoutDescriptor, immediateSize) == offsetof(WGPUPipelineLayoutDescriptor, immediateSize),
        "offsetof mismatch for PipelineLayoutDescriptor::immediateSize");

// PrimitiveState implementation

PrimitiveState::operator const WGPUPrimitiveState&() const noexcept {
    return *reinterpret_cast<const WGPUPrimitiveState*>(this);
}

static_assert(sizeof(PrimitiveState) == sizeof(WGPUPrimitiveState), "sizeof mismatch for PrimitiveState");
static_assert(alignof(PrimitiveState) == alignof(WGPUPrimitiveState), "alignof mismatch for PrimitiveState");
static_assert(offsetof(PrimitiveState, nextInChain) == offsetof(WGPUPrimitiveState, nextInChain),
        "offsetof mismatch for PrimitiveState::nextInChain");
static_assert(offsetof(PrimitiveState, topology) == offsetof(WGPUPrimitiveState, topology),
        "offsetof mismatch for PrimitiveState::topology");
static_assert(offsetof(PrimitiveState, stripIndexFormat) == offsetof(WGPUPrimitiveState, stripIndexFormat),
        "offsetof mismatch for PrimitiveState::stripIndexFormat");
static_assert(offsetof(PrimitiveState, frontFace) == offsetof(WGPUPrimitiveState, frontFace),
        "offsetof mismatch for PrimitiveState::frontFace");
static_assert(offsetof(PrimitiveState, cullMode) == offsetof(WGPUPrimitiveState, cullMode),
        "offsetof mismatch for PrimitiveState::cullMode");
static_assert(offsetof(PrimitiveState, unclippedDepth) == offsetof(WGPUPrimitiveState, unclippedDepth),
        "offsetof mismatch for PrimitiveState::unclippedDepth");

// QuerySetDescriptor implementation

QuerySetDescriptor::operator const WGPUQuerySetDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPUQuerySetDescriptor*>(this);
}

static_assert(sizeof(QuerySetDescriptor) == sizeof(WGPUQuerySetDescriptor), "sizeof mismatch for QuerySetDescriptor");
static_assert(alignof(QuerySetDescriptor) == alignof(WGPUQuerySetDescriptor), "alignof mismatch for QuerySetDescriptor");
static_assert(offsetof(QuerySetDescriptor, nextInChain) == offsetof(WGPUQuerySetDescriptor, nextInChain),
        "offsetof mismatch for QuerySetDescriptor::nextInChain");
static_assert(offsetof(QuerySetDescriptor, label) == offsetof(WGPUQuerySetDescriptor, label),
        "offsetof mismatch for QuerySetDescriptor::label");
static_assert(offsetof(QuerySetDescriptor, type) == offsetof(WGPUQuerySetDescriptor, type),
        "offsetof mismatch for QuerySetDescriptor::type");
static_assert(offsetof(QuerySetDescriptor, count) == offsetof(WGPUQuerySetDescriptor, count),
        "offsetof mismatch for QuerySetDescriptor::count");

// QueueDescriptor implementation

QueueDescriptor::operator const WGPUQueueDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPUQueueDescriptor*>(this);
}

static_assert(sizeof(QueueDescriptor) == sizeof(WGPUQueueDescriptor), "sizeof mismatch for QueueDescriptor");
static_assert(alignof(QueueDescriptor) == alignof(WGPUQueueDescriptor), "alignof mismatch for QueueDescriptor");
static_assert(offsetof(QueueDescriptor, nextInChain) == offsetof(WGPUQueueDescriptor, nextInChain),
        "offsetof mismatch for QueueDescriptor::nextInChain");
static_assert(offsetof(QueueDescriptor, label) == offsetof(WGPUQueueDescriptor, label),
        "offsetof mismatch for QueueDescriptor::label");

// RenderBundleDescriptor implementation

RenderBundleDescriptor::operator const WGPURenderBundleDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPURenderBundleDescriptor*>(this);
}

static_assert(sizeof(RenderBundleDescriptor) == sizeof(WGPURenderBundleDescriptor), "sizeof mismatch for RenderBundleDescriptor");
static_assert(alignof(RenderBundleDescriptor) == alignof(WGPURenderBundleDescriptor), "alignof mismatch for RenderBundleDescriptor");
static_assert(offsetof(RenderBundleDescriptor, nextInChain) == offsetof(WGPURenderBundleDescriptor, nextInChain),
        "offsetof mismatch for RenderBundleDescriptor::nextInChain");
static_assert(offsetof(RenderBundleDescriptor, label) == offsetof(WGPURenderBundleDescriptor, label),
        "offsetof mismatch for RenderBundleDescriptor::label");

// RenderBundleEncoderDescriptor implementation

RenderBundleEncoderDescriptor::operator const WGPURenderBundleEncoderDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPURenderBundleEncoderDescriptor*>(this);
}

static_assert(sizeof(RenderBundleEncoderDescriptor) == sizeof(WGPURenderBundleEncoderDescriptor), "sizeof mismatch for RenderBundleEncoderDescriptor");
static_assert(alignof(RenderBundleEncoderDescriptor) == alignof(WGPURenderBundleEncoderDescriptor), "alignof mismatch for RenderBundleEncoderDescriptor");
static_assert(offsetof(RenderBundleEncoderDescriptor, nextInChain) == offsetof(WGPURenderBundleEncoderDescriptor, nextInChain),
        "offsetof mismatch for RenderBundleEncoderDescriptor::nextInChain");
static_assert(offsetof(RenderBundleEncoderDescriptor, label) == offsetof(WGPURenderBundleEncoderDescriptor, label),
        "offsetof mismatch for RenderBundleEncoderDescriptor::label");
static_assert(offsetof(RenderBundleEncoderDescriptor, colorFormatCount) == offsetof(WGPURenderBundleEncoderDescriptor, colorFormatCount),
        "offsetof mismatch for RenderBundleEncoderDescriptor::colorFormatCount");
static_assert(offsetof(RenderBundleEncoderDescriptor, colorFormats) == offsetof(WGPURenderBundleEncoderDescriptor, colorFormats),
        "offsetof mismatch for RenderBundleEncoderDescriptor::colorFormats");
static_assert(offsetof(RenderBundleEncoderDescriptor, depthStencilFormat) == offsetof(WGPURenderBundleEncoderDescriptor, depthStencilFormat),
        "offsetof mismatch for RenderBundleEncoderDescriptor::depthStencilFormat");
static_assert(offsetof(RenderBundleEncoderDescriptor, sampleCount) == offsetof(WGPURenderBundleEncoderDescriptor, sampleCount),
        "offsetof mismatch for RenderBundleEncoderDescriptor::sampleCount");
static_assert(offsetof(RenderBundleEncoderDescriptor, depthReadOnly) == offsetof(WGPURenderBundleEncoderDescriptor, depthReadOnly),
        "offsetof mismatch for RenderBundleEncoderDescriptor::depthReadOnly");
static_assert(offsetof(RenderBundleEncoderDescriptor, stencilReadOnly) == offsetof(WGPURenderBundleEncoderDescriptor, stencilReadOnly),
        "offsetof mismatch for RenderBundleEncoderDescriptor::stencilReadOnly");

// RenderPassDepthStencilAttachment implementation

RenderPassDepthStencilAttachment::operator const WGPURenderPassDepthStencilAttachment&() const noexcept {
    return *reinterpret_cast<const WGPURenderPassDepthStencilAttachment*>(this);
}

static_assert(sizeof(RenderPassDepthStencilAttachment) == sizeof(WGPURenderPassDepthStencilAttachment), "sizeof mismatch for RenderPassDepthStencilAttachment");
static_assert(alignof(RenderPassDepthStencilAttachment) == alignof(WGPURenderPassDepthStencilAttachment), "alignof mismatch for RenderPassDepthStencilAttachment");
static_assert(offsetof(RenderPassDepthStencilAttachment, nextInChain) == offsetof(WGPURenderPassDepthStencilAttachment, nextInChain),
        "offsetof mismatch for RenderPassDepthStencilAttachment::nextInChain");
static_assert(offsetof(RenderPassDepthStencilAttachment, view) == offsetof(WGPURenderPassDepthStencilAttachment, view),
        "offsetof mismatch for RenderPassDepthStencilAttachment::view");
static_assert(offsetof(RenderPassDepthStencilAttachment, depthLoadOp) == offsetof(WGPURenderPassDepthStencilAttachment, depthLoadOp),
        "offsetof mismatch for RenderPassDepthStencilAttachment::depthLoadOp");
static_assert(offsetof(RenderPassDepthStencilAttachment, depthStoreOp) == offsetof(WGPURenderPassDepthStencilAttachment, depthStoreOp),
        "offsetof mismatch for RenderPassDepthStencilAttachment::depthStoreOp");
static_assert(offsetof(RenderPassDepthStencilAttachment, depthClearValue) == offsetof(WGPURenderPassDepthStencilAttachment, depthClearValue),
        "offsetof mismatch for RenderPassDepthStencilAttachment::depthClearValue");
static_assert(offsetof(RenderPassDepthStencilAttachment, depthReadOnly) == offsetof(WGPURenderPassDepthStencilAttachment, depthReadOnly),
        "offsetof mismatch for RenderPassDepthStencilAttachment::depthReadOnly");
static_assert(offsetof(RenderPassDepthStencilAttachment, stencilLoadOp) == offsetof(WGPURenderPassDepthStencilAttachment, stencilLoadOp),
        "offsetof mismatch for RenderPassDepthStencilAttachment::stencilLoadOp");
static_assert(offsetof(RenderPassDepthStencilAttachment, stencilStoreOp) == offsetof(WGPURenderPassDepthStencilAttachment, stencilStoreOp),
        "offsetof mismatch for RenderPassDepthStencilAttachment::stencilStoreOp");
static_assert(offsetof(RenderPassDepthStencilAttachment, stencilClearValue) == offsetof(WGPURenderPassDepthStencilAttachment, stencilClearValue),
        "offsetof mismatch for RenderPassDepthStencilAttachment::stencilClearValue");
static_assert(offsetof(RenderPassDepthStencilAttachment, stencilReadOnly) == offsetof(WGPURenderPassDepthStencilAttachment, stencilReadOnly),
        "offsetof mismatch for RenderPassDepthStencilAttachment::stencilReadOnly");

// RenderPassMaxDrawCount implementation
RenderPassMaxDrawCount::RenderPassMaxDrawCount()
  : ChainedStruct { nullptr, SType::RenderPassMaxDrawCount } {}
struct RenderPassMaxDrawCount::Init {
    ChainedStruct * const nextInChain;
    uint64_t maxDrawCount = 50000000;
};
RenderPassMaxDrawCount::RenderPassMaxDrawCount(RenderPassMaxDrawCount::Init&& init)
  : ChainedStruct { init.nextInChain, SType::RenderPassMaxDrawCount }, 
    maxDrawCount(std::move(init.maxDrawCount)){}

RenderPassMaxDrawCount::operator const WGPURenderPassMaxDrawCount&() const noexcept {
    return *reinterpret_cast<const WGPURenderPassMaxDrawCount*>(this);
}

static_assert(sizeof(RenderPassMaxDrawCount) == sizeof(WGPURenderPassMaxDrawCount), "sizeof mismatch for RenderPassMaxDrawCount");
static_assert(alignof(RenderPassMaxDrawCount) == alignof(WGPURenderPassMaxDrawCount), "alignof mismatch for RenderPassMaxDrawCount");
static_assert(offsetof(RenderPassMaxDrawCount, maxDrawCount) == offsetof(WGPURenderPassMaxDrawCount, maxDrawCount),
        "offsetof mismatch for RenderPassMaxDrawCount::maxDrawCount");

// RequestAdapterWebXROptions implementation
RequestAdapterWebXROptions::RequestAdapterWebXROptions()
  : ChainedStruct { nullptr, SType::RequestAdapterWebXROptions } {}
struct RequestAdapterWebXROptions::Init {
    ChainedStruct * const nextInChain;
    Bool xrCompatible;
};
RequestAdapterWebXROptions::RequestAdapterWebXROptions(RequestAdapterWebXROptions::Init&& init)
  : ChainedStruct { init.nextInChain, SType::RequestAdapterWebXROptions }, 
    xrCompatible(std::move(init.xrCompatible)){}

RequestAdapterWebXROptions::operator const WGPURequestAdapterWebXROptions&() const noexcept {
    return *reinterpret_cast<const WGPURequestAdapterWebXROptions*>(this);
}

static_assert(sizeof(RequestAdapterWebXROptions) == sizeof(WGPURequestAdapterWebXROptions), "sizeof mismatch for RequestAdapterWebXROptions");
static_assert(alignof(RequestAdapterWebXROptions) == alignof(WGPURequestAdapterWebXROptions), "alignof mismatch for RequestAdapterWebXROptions");
static_assert(offsetof(RequestAdapterWebXROptions, xrCompatible) == offsetof(WGPURequestAdapterWebXROptions, xrCompatible),
        "offsetof mismatch for RequestAdapterWebXROptions::xrCompatible");

// SamplerBindingLayout implementation

SamplerBindingLayout::operator const WGPUSamplerBindingLayout&() const noexcept {
    return *reinterpret_cast<const WGPUSamplerBindingLayout*>(this);
}

static_assert(sizeof(SamplerBindingLayout) == sizeof(WGPUSamplerBindingLayout), "sizeof mismatch for SamplerBindingLayout");
static_assert(alignof(SamplerBindingLayout) == alignof(WGPUSamplerBindingLayout), "alignof mismatch for SamplerBindingLayout");
static_assert(offsetof(SamplerBindingLayout, nextInChain) == offsetof(WGPUSamplerBindingLayout, nextInChain),
        "offsetof mismatch for SamplerBindingLayout::nextInChain");
static_assert(offsetof(SamplerBindingLayout, type) == offsetof(WGPUSamplerBindingLayout, type),
        "offsetof mismatch for SamplerBindingLayout::type");

// SamplerDescriptor implementation

SamplerDescriptor::operator const WGPUSamplerDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPUSamplerDescriptor*>(this);
}

static_assert(sizeof(SamplerDescriptor) == sizeof(WGPUSamplerDescriptor), "sizeof mismatch for SamplerDescriptor");
static_assert(alignof(SamplerDescriptor) == alignof(WGPUSamplerDescriptor), "alignof mismatch for SamplerDescriptor");
static_assert(offsetof(SamplerDescriptor, nextInChain) == offsetof(WGPUSamplerDescriptor, nextInChain),
        "offsetof mismatch for SamplerDescriptor::nextInChain");
static_assert(offsetof(SamplerDescriptor, label) == offsetof(WGPUSamplerDescriptor, label),
        "offsetof mismatch for SamplerDescriptor::label");
static_assert(offsetof(SamplerDescriptor, addressModeU) == offsetof(WGPUSamplerDescriptor, addressModeU),
        "offsetof mismatch for SamplerDescriptor::addressModeU");
static_assert(offsetof(SamplerDescriptor, addressModeV) == offsetof(WGPUSamplerDescriptor, addressModeV),
        "offsetof mismatch for SamplerDescriptor::addressModeV");
static_assert(offsetof(SamplerDescriptor, addressModeW) == offsetof(WGPUSamplerDescriptor, addressModeW),
        "offsetof mismatch for SamplerDescriptor::addressModeW");
static_assert(offsetof(SamplerDescriptor, magFilter) == offsetof(WGPUSamplerDescriptor, magFilter),
        "offsetof mismatch for SamplerDescriptor::magFilter");
static_assert(offsetof(SamplerDescriptor, minFilter) == offsetof(WGPUSamplerDescriptor, minFilter),
        "offsetof mismatch for SamplerDescriptor::minFilter");
static_assert(offsetof(SamplerDescriptor, mipmapFilter) == offsetof(WGPUSamplerDescriptor, mipmapFilter),
        "offsetof mismatch for SamplerDescriptor::mipmapFilter");
static_assert(offsetof(SamplerDescriptor, lodMinClamp) == offsetof(WGPUSamplerDescriptor, lodMinClamp),
        "offsetof mismatch for SamplerDescriptor::lodMinClamp");
static_assert(offsetof(SamplerDescriptor, lodMaxClamp) == offsetof(WGPUSamplerDescriptor, lodMaxClamp),
        "offsetof mismatch for SamplerDescriptor::lodMaxClamp");
static_assert(offsetof(SamplerDescriptor, compare) == offsetof(WGPUSamplerDescriptor, compare),
        "offsetof mismatch for SamplerDescriptor::compare");
static_assert(offsetof(SamplerDescriptor, maxAnisotropy) == offsetof(WGPUSamplerDescriptor, maxAnisotropy),
        "offsetof mismatch for SamplerDescriptor::maxAnisotropy");

// ShaderSourceSPIRV implementation
ShaderSourceSPIRV::ShaderSourceSPIRV()
  : ChainedStruct { nullptr, SType::ShaderSourceSPIRV } {}
struct ShaderSourceSPIRV::Init {
    ChainedStruct * const nextInChain;
    uint32_t codeSize;
    uint32_t const * code = nullptr;
};
ShaderSourceSPIRV::ShaderSourceSPIRV(ShaderSourceSPIRV::Init&& init)
  : ChainedStruct { init.nextInChain, SType::ShaderSourceSPIRV }, 
    codeSize(std::move(init.codeSize)), 
    code(std::move(init.code)){}

ShaderSourceSPIRV::operator const WGPUShaderSourceSPIRV&() const noexcept {
    return *reinterpret_cast<const WGPUShaderSourceSPIRV*>(this);
}

static_assert(sizeof(ShaderSourceSPIRV) == sizeof(WGPUShaderSourceSPIRV), "sizeof mismatch for ShaderSourceSPIRV");
static_assert(alignof(ShaderSourceSPIRV) == alignof(WGPUShaderSourceSPIRV), "alignof mismatch for ShaderSourceSPIRV");
static_assert(offsetof(ShaderSourceSPIRV, codeSize) == offsetof(WGPUShaderSourceSPIRV, codeSize),
        "offsetof mismatch for ShaderSourceSPIRV::codeSize");
static_assert(offsetof(ShaderSourceSPIRV, code) == offsetof(WGPUShaderSourceSPIRV, code),
        "offsetof mismatch for ShaderSourceSPIRV::code");

// ShaderSourceWGSL implementation
ShaderSourceWGSL::ShaderSourceWGSL()
  : ChainedStruct { nullptr, SType::ShaderSourceWGSL } {}
struct ShaderSourceWGSL::Init {
    ChainedStruct * const nextInChain;
    StringView code = {};
};
ShaderSourceWGSL::ShaderSourceWGSL(ShaderSourceWGSL::Init&& init)
  : ChainedStruct { init.nextInChain, SType::ShaderSourceWGSL }, 
    code(std::move(init.code)){}

ShaderSourceWGSL::operator const WGPUShaderSourceWGSL&() const noexcept {
    return *reinterpret_cast<const WGPUShaderSourceWGSL*>(this);
}

static_assert(sizeof(ShaderSourceWGSL) == sizeof(WGPUShaderSourceWGSL), "sizeof mismatch for ShaderSourceWGSL");
static_assert(alignof(ShaderSourceWGSL) == alignof(WGPUShaderSourceWGSL), "alignof mismatch for ShaderSourceWGSL");
static_assert(offsetof(ShaderSourceWGSL, code) == offsetof(WGPUShaderSourceWGSL, code),
        "offsetof mismatch for ShaderSourceWGSL::code");

// StencilFaceState implementation

StencilFaceState::operator const WGPUStencilFaceState&() const noexcept {
    return *reinterpret_cast<const WGPUStencilFaceState*>(this);
}

static_assert(sizeof(StencilFaceState) == sizeof(WGPUStencilFaceState), "sizeof mismatch for StencilFaceState");
static_assert(alignof(StencilFaceState) == alignof(WGPUStencilFaceState), "alignof mismatch for StencilFaceState");
static_assert(offsetof(StencilFaceState, compare) == offsetof(WGPUStencilFaceState, compare),
        "offsetof mismatch for StencilFaceState::compare");
static_assert(offsetof(StencilFaceState, failOp) == offsetof(WGPUStencilFaceState, failOp),
        "offsetof mismatch for StencilFaceState::failOp");
static_assert(offsetof(StencilFaceState, depthFailOp) == offsetof(WGPUStencilFaceState, depthFailOp),
        "offsetof mismatch for StencilFaceState::depthFailOp");
static_assert(offsetof(StencilFaceState, passOp) == offsetof(WGPUStencilFaceState, passOp),
        "offsetof mismatch for StencilFaceState::passOp");

// StorageTextureBindingLayout implementation

StorageTextureBindingLayout::operator const WGPUStorageTextureBindingLayout&() const noexcept {
    return *reinterpret_cast<const WGPUStorageTextureBindingLayout*>(this);
}

static_assert(sizeof(StorageTextureBindingLayout) == sizeof(WGPUStorageTextureBindingLayout), "sizeof mismatch for StorageTextureBindingLayout");
static_assert(alignof(StorageTextureBindingLayout) == alignof(WGPUStorageTextureBindingLayout), "alignof mismatch for StorageTextureBindingLayout");
static_assert(offsetof(StorageTextureBindingLayout, nextInChain) == offsetof(WGPUStorageTextureBindingLayout, nextInChain),
        "offsetof mismatch for StorageTextureBindingLayout::nextInChain");
static_assert(offsetof(StorageTextureBindingLayout, access) == offsetof(WGPUStorageTextureBindingLayout, access),
        "offsetof mismatch for StorageTextureBindingLayout::access");
static_assert(offsetof(StorageTextureBindingLayout, format) == offsetof(WGPUStorageTextureBindingLayout, format),
        "offsetof mismatch for StorageTextureBindingLayout::format");
static_assert(offsetof(StorageTextureBindingLayout, viewDimension) == offsetof(WGPUStorageTextureBindingLayout, viewDimension),
        "offsetof mismatch for StorageTextureBindingLayout::viewDimension");

// SupportedFeatures implementation
SupportedFeatures::SupportedFeatures() = default;
SupportedFeatures::~SupportedFeatures() {
    FreeMembers();
}

SupportedFeatures::SupportedFeatures(SupportedFeatures&& rhs)
    : featureCount(rhs.featureCount),
            features(rhs.features){
    Reset(rhs);
}

SupportedFeatures& SupportedFeatures::operator=(SupportedFeatures&& rhs) {
    if (&rhs == this) {
        return *this;
    }
    FreeMembers();
    detail::AsNonConstReference(this->featureCount) = std::move(rhs.featureCount);
    detail::AsNonConstReference(this->features) = std::move(rhs.features);
    Reset(rhs);
    return *this;
}

void SupportedFeatures::FreeMembers() {
    bool needsFreeing = false;    if (this->features != nullptr) { needsFreeing = true; }if (needsFreeing) {
        wgpuSupportedFeaturesFreeMembers(
            *reinterpret_cast<WGPUSupportedFeatures*>(this));
    }
}

// static
void SupportedFeatures::Reset(SupportedFeatures& value) {
    SupportedFeatures defaultValue{};
    detail::AsNonConstReference(value.featureCount) = defaultValue.featureCount;
    detail::AsNonConstReference(value.features) = defaultValue.features;
}

SupportedFeatures::operator const WGPUSupportedFeatures&() const noexcept {
    return *reinterpret_cast<const WGPUSupportedFeatures*>(this);
}

static_assert(sizeof(SupportedFeatures) == sizeof(WGPUSupportedFeatures), "sizeof mismatch for SupportedFeatures");
static_assert(alignof(SupportedFeatures) == alignof(WGPUSupportedFeatures), "alignof mismatch for SupportedFeatures");
static_assert(offsetof(SupportedFeatures, featureCount) == offsetof(WGPUSupportedFeatures, featureCount),
        "offsetof mismatch for SupportedFeatures::featureCount");
static_assert(offsetof(SupportedFeatures, features) == offsetof(WGPUSupportedFeatures, features),
        "offsetof mismatch for SupportedFeatures::features");

// SupportedInstanceFeatures implementation
SupportedInstanceFeatures::SupportedInstanceFeatures() = default;
SupportedInstanceFeatures::~SupportedInstanceFeatures() {
    FreeMembers();
}

SupportedInstanceFeatures::SupportedInstanceFeatures(SupportedInstanceFeatures&& rhs)
    : featureCount(rhs.featureCount),
            features(rhs.features){
    Reset(rhs);
}

SupportedInstanceFeatures& SupportedInstanceFeatures::operator=(SupportedInstanceFeatures&& rhs) {
    if (&rhs == this) {
        return *this;
    }
    FreeMembers();
    detail::AsNonConstReference(this->featureCount) = std::move(rhs.featureCount);
    detail::AsNonConstReference(this->features) = std::move(rhs.features);
    Reset(rhs);
    return *this;
}

void SupportedInstanceFeatures::FreeMembers() {
    bool needsFreeing = false;    if (this->features != nullptr) { needsFreeing = true; }if (needsFreeing) {
        wgpuSupportedInstanceFeaturesFreeMembers(
            *reinterpret_cast<WGPUSupportedInstanceFeatures*>(this));
    }
}

// static
void SupportedInstanceFeatures::Reset(SupportedInstanceFeatures& value) {
    SupportedInstanceFeatures defaultValue{};
    detail::AsNonConstReference(value.featureCount) = defaultValue.featureCount;
    detail::AsNonConstReference(value.features) = defaultValue.features;
}

SupportedInstanceFeatures::operator const WGPUSupportedInstanceFeatures&() const noexcept {
    return *reinterpret_cast<const WGPUSupportedInstanceFeatures*>(this);
}

static_assert(sizeof(SupportedInstanceFeatures) == sizeof(WGPUSupportedInstanceFeatures), "sizeof mismatch for SupportedInstanceFeatures");
static_assert(alignof(SupportedInstanceFeatures) == alignof(WGPUSupportedInstanceFeatures), "alignof mismatch for SupportedInstanceFeatures");
static_assert(offsetof(SupportedInstanceFeatures, featureCount) == offsetof(WGPUSupportedInstanceFeatures, featureCount),
        "offsetof mismatch for SupportedInstanceFeatures::featureCount");
static_assert(offsetof(SupportedInstanceFeatures, features) == offsetof(WGPUSupportedInstanceFeatures, features),
        "offsetof mismatch for SupportedInstanceFeatures::features");

// SupportedWGSLLanguageFeatures implementation
SupportedWGSLLanguageFeatures::SupportedWGSLLanguageFeatures() = default;
SupportedWGSLLanguageFeatures::~SupportedWGSLLanguageFeatures() {
    FreeMembers();
}

SupportedWGSLLanguageFeatures::SupportedWGSLLanguageFeatures(SupportedWGSLLanguageFeatures&& rhs)
    : featureCount(rhs.featureCount),
            features(rhs.features){
    Reset(rhs);
}

SupportedWGSLLanguageFeatures& SupportedWGSLLanguageFeatures::operator=(SupportedWGSLLanguageFeatures&& rhs) {
    if (&rhs == this) {
        return *this;
    }
    FreeMembers();
    detail::AsNonConstReference(this->featureCount) = std::move(rhs.featureCount);
    detail::AsNonConstReference(this->features) = std::move(rhs.features);
    Reset(rhs);
    return *this;
}

void SupportedWGSLLanguageFeatures::FreeMembers() {
    bool needsFreeing = false;    if (this->features != nullptr) { needsFreeing = true; }if (needsFreeing) {
        wgpuSupportedWGSLLanguageFeaturesFreeMembers(
            *reinterpret_cast<WGPUSupportedWGSLLanguageFeatures*>(this));
    }
}

// static
void SupportedWGSLLanguageFeatures::Reset(SupportedWGSLLanguageFeatures& value) {
    SupportedWGSLLanguageFeatures defaultValue{};
    detail::AsNonConstReference(value.featureCount) = defaultValue.featureCount;
    detail::AsNonConstReference(value.features) = defaultValue.features;
}

SupportedWGSLLanguageFeatures::operator const WGPUSupportedWGSLLanguageFeatures&() const noexcept {
    return *reinterpret_cast<const WGPUSupportedWGSLLanguageFeatures*>(this);
}

static_assert(sizeof(SupportedWGSLLanguageFeatures) == sizeof(WGPUSupportedWGSLLanguageFeatures), "sizeof mismatch for SupportedWGSLLanguageFeatures");
static_assert(alignof(SupportedWGSLLanguageFeatures) == alignof(WGPUSupportedWGSLLanguageFeatures), "alignof mismatch for SupportedWGSLLanguageFeatures");
static_assert(offsetof(SupportedWGSLLanguageFeatures, featureCount) == offsetof(WGPUSupportedWGSLLanguageFeatures, featureCount),
        "offsetof mismatch for SupportedWGSLLanguageFeatures::featureCount");
static_assert(offsetof(SupportedWGSLLanguageFeatures, features) == offsetof(WGPUSupportedWGSLLanguageFeatures, features),
        "offsetof mismatch for SupportedWGSLLanguageFeatures::features");

// SurfaceCapabilities implementation
SurfaceCapabilities::SurfaceCapabilities() = default;
SurfaceCapabilities::~SurfaceCapabilities() {
    FreeMembers();
}

SurfaceCapabilities::SurfaceCapabilities(SurfaceCapabilities&& rhs)
    : usages(rhs.usages),
            formatCount(rhs.formatCount),
            formats(rhs.formats),
            presentModeCount(rhs.presentModeCount),
            presentModes(rhs.presentModes),
            alphaModeCount(rhs.alphaModeCount),
            alphaModes(rhs.alphaModes){
    Reset(rhs);
}

SurfaceCapabilities& SurfaceCapabilities::operator=(SurfaceCapabilities&& rhs) {
    if (&rhs == this) {
        return *this;
    }
    FreeMembers();
    detail::AsNonConstReference(this->usages) = std::move(rhs.usages);
    detail::AsNonConstReference(this->formatCount) = std::move(rhs.formatCount);
    detail::AsNonConstReference(this->formats) = std::move(rhs.formats);
    detail::AsNonConstReference(this->presentModeCount) = std::move(rhs.presentModeCount);
    detail::AsNonConstReference(this->presentModes) = std::move(rhs.presentModes);
    detail::AsNonConstReference(this->alphaModeCount) = std::move(rhs.alphaModeCount);
    detail::AsNonConstReference(this->alphaModes) = std::move(rhs.alphaModes);
    Reset(rhs);
    return *this;
}

void SurfaceCapabilities::FreeMembers() {
    bool needsFreeing = false;    if (this->formats != nullptr) { needsFreeing = true; }    if (this->presentModes != nullptr) { needsFreeing = true; }    if (this->alphaModes != nullptr) { needsFreeing = true; }if (needsFreeing) {
        wgpuSurfaceCapabilitiesFreeMembers(
            *reinterpret_cast<WGPUSurfaceCapabilities*>(this));
    }
}

// static
void SurfaceCapabilities::Reset(SurfaceCapabilities& value) {
    SurfaceCapabilities defaultValue{};
    detail::AsNonConstReference(value.usages) = defaultValue.usages;
    detail::AsNonConstReference(value.formatCount) = defaultValue.formatCount;
    detail::AsNonConstReference(value.formats) = defaultValue.formats;
    detail::AsNonConstReference(value.presentModeCount) = defaultValue.presentModeCount;
    detail::AsNonConstReference(value.presentModes) = defaultValue.presentModes;
    detail::AsNonConstReference(value.alphaModeCount) = defaultValue.alphaModeCount;
    detail::AsNonConstReference(value.alphaModes) = defaultValue.alphaModes;
}

SurfaceCapabilities::operator const WGPUSurfaceCapabilities&() const noexcept {
    return *reinterpret_cast<const WGPUSurfaceCapabilities*>(this);
}

static_assert(sizeof(SurfaceCapabilities) == sizeof(WGPUSurfaceCapabilities), "sizeof mismatch for SurfaceCapabilities");
static_assert(alignof(SurfaceCapabilities) == alignof(WGPUSurfaceCapabilities), "alignof mismatch for SurfaceCapabilities");
static_assert(offsetof(SurfaceCapabilities, nextInChain) == offsetof(WGPUSurfaceCapabilities, nextInChain),
        "offsetof mismatch for SurfaceCapabilities::nextInChain");
static_assert(offsetof(SurfaceCapabilities, usages) == offsetof(WGPUSurfaceCapabilities, usages),
        "offsetof mismatch for SurfaceCapabilities::usages");
static_assert(offsetof(SurfaceCapabilities, formatCount) == offsetof(WGPUSurfaceCapabilities, formatCount),
        "offsetof mismatch for SurfaceCapabilities::formatCount");
static_assert(offsetof(SurfaceCapabilities, formats) == offsetof(WGPUSurfaceCapabilities, formats),
        "offsetof mismatch for SurfaceCapabilities::formats");
static_assert(offsetof(SurfaceCapabilities, presentModeCount) == offsetof(WGPUSurfaceCapabilities, presentModeCount),
        "offsetof mismatch for SurfaceCapabilities::presentModeCount");
static_assert(offsetof(SurfaceCapabilities, presentModes) == offsetof(WGPUSurfaceCapabilities, presentModes),
        "offsetof mismatch for SurfaceCapabilities::presentModes");
static_assert(offsetof(SurfaceCapabilities, alphaModeCount) == offsetof(WGPUSurfaceCapabilities, alphaModeCount),
        "offsetof mismatch for SurfaceCapabilities::alphaModeCount");
static_assert(offsetof(SurfaceCapabilities, alphaModes) == offsetof(WGPUSurfaceCapabilities, alphaModes),
        "offsetof mismatch for SurfaceCapabilities::alphaModes");

// SurfaceColorManagement implementation
SurfaceColorManagement::SurfaceColorManagement()
  : ChainedStruct { nullptr, SType::SurfaceColorManagement } {}
struct SurfaceColorManagement::Init {
    ChainedStruct * const nextInChain;
    PredefinedColorSpace colorSpace = {};
    ToneMappingMode toneMappingMode = {};
};
SurfaceColorManagement::SurfaceColorManagement(SurfaceColorManagement::Init&& init)
  : ChainedStruct { init.nextInChain, SType::SurfaceColorManagement }, 
    colorSpace(std::move(init.colorSpace)), 
    toneMappingMode(std::move(init.toneMappingMode)){}

SurfaceColorManagement::operator const WGPUSurfaceColorManagement&() const noexcept {
    return *reinterpret_cast<const WGPUSurfaceColorManagement*>(this);
}

static_assert(sizeof(SurfaceColorManagement) == sizeof(WGPUSurfaceColorManagement), "sizeof mismatch for SurfaceColorManagement");
static_assert(alignof(SurfaceColorManagement) == alignof(WGPUSurfaceColorManagement), "alignof mismatch for SurfaceColorManagement");
static_assert(offsetof(SurfaceColorManagement, colorSpace) == offsetof(WGPUSurfaceColorManagement, colorSpace),
        "offsetof mismatch for SurfaceColorManagement::colorSpace");
static_assert(offsetof(SurfaceColorManagement, toneMappingMode) == offsetof(WGPUSurfaceColorManagement, toneMappingMode),
        "offsetof mismatch for SurfaceColorManagement::toneMappingMode");

// SurfaceConfiguration implementation

SurfaceConfiguration::operator const WGPUSurfaceConfiguration&() const noexcept {
    return *reinterpret_cast<const WGPUSurfaceConfiguration*>(this);
}

static_assert(sizeof(SurfaceConfiguration) == sizeof(WGPUSurfaceConfiguration), "sizeof mismatch for SurfaceConfiguration");
static_assert(alignof(SurfaceConfiguration) == alignof(WGPUSurfaceConfiguration), "alignof mismatch for SurfaceConfiguration");
static_assert(offsetof(SurfaceConfiguration, nextInChain) == offsetof(WGPUSurfaceConfiguration, nextInChain),
        "offsetof mismatch for SurfaceConfiguration::nextInChain");
static_assert(offsetof(SurfaceConfiguration, device) == offsetof(WGPUSurfaceConfiguration, device),
        "offsetof mismatch for SurfaceConfiguration::device");
static_assert(offsetof(SurfaceConfiguration, format) == offsetof(WGPUSurfaceConfiguration, format),
        "offsetof mismatch for SurfaceConfiguration::format");
static_assert(offsetof(SurfaceConfiguration, usage) == offsetof(WGPUSurfaceConfiguration, usage),
        "offsetof mismatch for SurfaceConfiguration::usage");
static_assert(offsetof(SurfaceConfiguration, width) == offsetof(WGPUSurfaceConfiguration, width),
        "offsetof mismatch for SurfaceConfiguration::width");
static_assert(offsetof(SurfaceConfiguration, height) == offsetof(WGPUSurfaceConfiguration, height),
        "offsetof mismatch for SurfaceConfiguration::height");
static_assert(offsetof(SurfaceConfiguration, viewFormatCount) == offsetof(WGPUSurfaceConfiguration, viewFormatCount),
        "offsetof mismatch for SurfaceConfiguration::viewFormatCount");
static_assert(offsetof(SurfaceConfiguration, viewFormats) == offsetof(WGPUSurfaceConfiguration, viewFormats),
        "offsetof mismatch for SurfaceConfiguration::viewFormats");
static_assert(offsetof(SurfaceConfiguration, alphaMode) == offsetof(WGPUSurfaceConfiguration, alphaMode),
        "offsetof mismatch for SurfaceConfiguration::alphaMode");
static_assert(offsetof(SurfaceConfiguration, presentMode) == offsetof(WGPUSurfaceConfiguration, presentMode),
        "offsetof mismatch for SurfaceConfiguration::presentMode");

// SurfaceTexture implementation

SurfaceTexture::operator const WGPUSurfaceTexture&() const noexcept {
    return *reinterpret_cast<const WGPUSurfaceTexture*>(this);
}

static_assert(sizeof(SurfaceTexture) == sizeof(WGPUSurfaceTexture), "sizeof mismatch for SurfaceTexture");
static_assert(alignof(SurfaceTexture) == alignof(WGPUSurfaceTexture), "alignof mismatch for SurfaceTexture");
static_assert(offsetof(SurfaceTexture, nextInChain) == offsetof(WGPUSurfaceTexture, nextInChain),
        "offsetof mismatch for SurfaceTexture::nextInChain");
static_assert(offsetof(SurfaceTexture, texture) == offsetof(WGPUSurfaceTexture, texture),
        "offsetof mismatch for SurfaceTexture::texture");
static_assert(offsetof(SurfaceTexture, status) == offsetof(WGPUSurfaceTexture, status),
        "offsetof mismatch for SurfaceTexture::status");

// TexelCopyBufferLayout implementation

TexelCopyBufferLayout::operator const WGPUTexelCopyBufferLayout&() const noexcept {
    return *reinterpret_cast<const WGPUTexelCopyBufferLayout*>(this);
}

static_assert(sizeof(TexelCopyBufferLayout) == sizeof(WGPUTexelCopyBufferLayout), "sizeof mismatch for TexelCopyBufferLayout");
static_assert(alignof(TexelCopyBufferLayout) == alignof(WGPUTexelCopyBufferLayout), "alignof mismatch for TexelCopyBufferLayout");
static_assert(offsetof(TexelCopyBufferLayout, offset) == offsetof(WGPUTexelCopyBufferLayout, offset),
        "offsetof mismatch for TexelCopyBufferLayout::offset");
static_assert(offsetof(TexelCopyBufferLayout, bytesPerRow) == offsetof(WGPUTexelCopyBufferLayout, bytesPerRow),
        "offsetof mismatch for TexelCopyBufferLayout::bytesPerRow");
static_assert(offsetof(TexelCopyBufferLayout, rowsPerImage) == offsetof(WGPUTexelCopyBufferLayout, rowsPerImage),
        "offsetof mismatch for TexelCopyBufferLayout::rowsPerImage");

// TextureBindingLayout implementation

TextureBindingLayout::operator const WGPUTextureBindingLayout&() const noexcept {
    return *reinterpret_cast<const WGPUTextureBindingLayout*>(this);
}

static_assert(sizeof(TextureBindingLayout) == sizeof(WGPUTextureBindingLayout), "sizeof mismatch for TextureBindingLayout");
static_assert(alignof(TextureBindingLayout) == alignof(WGPUTextureBindingLayout), "alignof mismatch for TextureBindingLayout");
static_assert(offsetof(TextureBindingLayout, nextInChain) == offsetof(WGPUTextureBindingLayout, nextInChain),
        "offsetof mismatch for TextureBindingLayout::nextInChain");
static_assert(offsetof(TextureBindingLayout, sampleType) == offsetof(WGPUTextureBindingLayout, sampleType),
        "offsetof mismatch for TextureBindingLayout::sampleType");
static_assert(offsetof(TextureBindingLayout, viewDimension) == offsetof(WGPUTextureBindingLayout, viewDimension),
        "offsetof mismatch for TextureBindingLayout::viewDimension");
static_assert(offsetof(TextureBindingLayout, multisampled) == offsetof(WGPUTextureBindingLayout, multisampled),
        "offsetof mismatch for TextureBindingLayout::multisampled");

// TextureBindingViewDimensionDescriptor implementation
TextureBindingViewDimensionDescriptor::TextureBindingViewDimensionDescriptor()
  : ChainedStruct { nullptr, SType::TextureBindingViewDimensionDescriptor } {}
struct TextureBindingViewDimensionDescriptor::Init {
    ChainedStruct * const nextInChain;
    TextureViewDimension textureBindingViewDimension = TextureViewDimension::Undefined;
};
TextureBindingViewDimensionDescriptor::TextureBindingViewDimensionDescriptor(TextureBindingViewDimensionDescriptor::Init&& init)
  : ChainedStruct { init.nextInChain, SType::TextureBindingViewDimensionDescriptor }, 
    textureBindingViewDimension(std::move(init.textureBindingViewDimension)){}

TextureBindingViewDimensionDescriptor::operator const WGPUTextureBindingViewDimensionDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPUTextureBindingViewDimensionDescriptor*>(this);
}

static_assert(sizeof(TextureBindingViewDimensionDescriptor) == sizeof(WGPUTextureBindingViewDimensionDescriptor), "sizeof mismatch for TextureBindingViewDimensionDescriptor");
static_assert(alignof(TextureBindingViewDimensionDescriptor) == alignof(WGPUTextureBindingViewDimensionDescriptor), "alignof mismatch for TextureBindingViewDimensionDescriptor");
static_assert(offsetof(TextureBindingViewDimensionDescriptor, textureBindingViewDimension) == offsetof(WGPUTextureBindingViewDimensionDescriptor, textureBindingViewDimension),
        "offsetof mismatch for TextureBindingViewDimensionDescriptor::textureBindingViewDimension");

// TextureComponentSwizzle implementation

TextureComponentSwizzle::operator const WGPUTextureComponentSwizzle&() const noexcept {
    return *reinterpret_cast<const WGPUTextureComponentSwizzle*>(this);
}

static_assert(sizeof(TextureComponentSwizzle) == sizeof(WGPUTextureComponentSwizzle), "sizeof mismatch for TextureComponentSwizzle");
static_assert(alignof(TextureComponentSwizzle) == alignof(WGPUTextureComponentSwizzle), "alignof mismatch for TextureComponentSwizzle");
static_assert(offsetof(TextureComponentSwizzle, r) == offsetof(WGPUTextureComponentSwizzle, r),
        "offsetof mismatch for TextureComponentSwizzle::r");
static_assert(offsetof(TextureComponentSwizzle, g) == offsetof(WGPUTextureComponentSwizzle, g),
        "offsetof mismatch for TextureComponentSwizzle::g");
static_assert(offsetof(TextureComponentSwizzle, b) == offsetof(WGPUTextureComponentSwizzle, b),
        "offsetof mismatch for TextureComponentSwizzle::b");
static_assert(offsetof(TextureComponentSwizzle, a) == offsetof(WGPUTextureComponentSwizzle, a),
        "offsetof mismatch for TextureComponentSwizzle::a");

// VertexAttribute implementation

VertexAttribute::operator const WGPUVertexAttribute&() const noexcept {
    return *reinterpret_cast<const WGPUVertexAttribute*>(this);
}

static_assert(sizeof(VertexAttribute) == sizeof(WGPUVertexAttribute), "sizeof mismatch for VertexAttribute");
static_assert(alignof(VertexAttribute) == alignof(WGPUVertexAttribute), "alignof mismatch for VertexAttribute");
static_assert(offsetof(VertexAttribute, nextInChain) == offsetof(WGPUVertexAttribute, nextInChain),
        "offsetof mismatch for VertexAttribute::nextInChain");
static_assert(offsetof(VertexAttribute, format) == offsetof(WGPUVertexAttribute, format),
        "offsetof mismatch for VertexAttribute::format");
static_assert(offsetof(VertexAttribute, offset) == offsetof(WGPUVertexAttribute, offset),
        "offsetof mismatch for VertexAttribute::offset");
static_assert(offsetof(VertexAttribute, shaderLocation) == offsetof(WGPUVertexAttribute, shaderLocation),
        "offsetof mismatch for VertexAttribute::shaderLocation");

// BindGroupDescriptor implementation

BindGroupDescriptor::operator const WGPUBindGroupDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPUBindGroupDescriptor*>(this);
}

static_assert(sizeof(BindGroupDescriptor) == sizeof(WGPUBindGroupDescriptor), "sizeof mismatch for BindGroupDescriptor");
static_assert(alignof(BindGroupDescriptor) == alignof(WGPUBindGroupDescriptor), "alignof mismatch for BindGroupDescriptor");
static_assert(offsetof(BindGroupDescriptor, nextInChain) == offsetof(WGPUBindGroupDescriptor, nextInChain),
        "offsetof mismatch for BindGroupDescriptor::nextInChain");
static_assert(offsetof(BindGroupDescriptor, label) == offsetof(WGPUBindGroupDescriptor, label),
        "offsetof mismatch for BindGroupDescriptor::label");
static_assert(offsetof(BindGroupDescriptor, layout) == offsetof(WGPUBindGroupDescriptor, layout),
        "offsetof mismatch for BindGroupDescriptor::layout");
static_assert(offsetof(BindGroupDescriptor, entryCount) == offsetof(WGPUBindGroupDescriptor, entryCount),
        "offsetof mismatch for BindGroupDescriptor::entryCount");
static_assert(offsetof(BindGroupDescriptor, entries) == offsetof(WGPUBindGroupDescriptor, entries),
        "offsetof mismatch for BindGroupDescriptor::entries");

// BindGroupLayoutEntry implementation

BindGroupLayoutEntry::operator const WGPUBindGroupLayoutEntry&() const noexcept {
    return *reinterpret_cast<const WGPUBindGroupLayoutEntry*>(this);
}

static_assert(sizeof(BindGroupLayoutEntry) == sizeof(WGPUBindGroupLayoutEntry), "sizeof mismatch for BindGroupLayoutEntry");
static_assert(alignof(BindGroupLayoutEntry) == alignof(WGPUBindGroupLayoutEntry), "alignof mismatch for BindGroupLayoutEntry");
static_assert(offsetof(BindGroupLayoutEntry, nextInChain) == offsetof(WGPUBindGroupLayoutEntry, nextInChain),
        "offsetof mismatch for BindGroupLayoutEntry::nextInChain");
static_assert(offsetof(BindGroupLayoutEntry, binding) == offsetof(WGPUBindGroupLayoutEntry, binding),
        "offsetof mismatch for BindGroupLayoutEntry::binding");
static_assert(offsetof(BindGroupLayoutEntry, visibility) == offsetof(WGPUBindGroupLayoutEntry, visibility),
        "offsetof mismatch for BindGroupLayoutEntry::visibility");
static_assert(offsetof(BindGroupLayoutEntry, bindingArraySize) == offsetof(WGPUBindGroupLayoutEntry, bindingArraySize),
        "offsetof mismatch for BindGroupLayoutEntry::bindingArraySize");
static_assert(offsetof(BindGroupLayoutEntry, buffer) == offsetof(WGPUBindGroupLayoutEntry, buffer),
        "offsetof mismatch for BindGroupLayoutEntry::buffer");
static_assert(offsetof(BindGroupLayoutEntry, sampler) == offsetof(WGPUBindGroupLayoutEntry, sampler),
        "offsetof mismatch for BindGroupLayoutEntry::sampler");
static_assert(offsetof(BindGroupLayoutEntry, texture) == offsetof(WGPUBindGroupLayoutEntry, texture),
        "offsetof mismatch for BindGroupLayoutEntry::texture");
static_assert(offsetof(BindGroupLayoutEntry, storageTexture) == offsetof(WGPUBindGroupLayoutEntry, storageTexture),
        "offsetof mismatch for BindGroupLayoutEntry::storageTexture");

// BlendState implementation

BlendState::operator const WGPUBlendState&() const noexcept {
    return *reinterpret_cast<const WGPUBlendState*>(this);
}

static_assert(sizeof(BlendState) == sizeof(WGPUBlendState), "sizeof mismatch for BlendState");
static_assert(alignof(BlendState) == alignof(WGPUBlendState), "alignof mismatch for BlendState");
static_assert(offsetof(BlendState, color) == offsetof(WGPUBlendState, color),
        "offsetof mismatch for BlendState::color");
static_assert(offsetof(BlendState, alpha) == offsetof(WGPUBlendState, alpha),
        "offsetof mismatch for BlendState::alpha");

// CompilationMessage implementation

CompilationMessage::operator const WGPUCompilationMessage&() const noexcept {
    return *reinterpret_cast<const WGPUCompilationMessage*>(this);
}

static_assert(sizeof(CompilationMessage) == sizeof(WGPUCompilationMessage), "sizeof mismatch for CompilationMessage");
static_assert(alignof(CompilationMessage) == alignof(WGPUCompilationMessage), "alignof mismatch for CompilationMessage");
static_assert(offsetof(CompilationMessage, nextInChain) == offsetof(WGPUCompilationMessage, nextInChain),
        "offsetof mismatch for CompilationMessage::nextInChain");
static_assert(offsetof(CompilationMessage, message) == offsetof(WGPUCompilationMessage, message),
        "offsetof mismatch for CompilationMessage::message");
static_assert(offsetof(CompilationMessage, type) == offsetof(WGPUCompilationMessage, type),
        "offsetof mismatch for CompilationMessage::type");
static_assert(offsetof(CompilationMessage, lineNum) == offsetof(WGPUCompilationMessage, lineNum),
        "offsetof mismatch for CompilationMessage::lineNum");
static_assert(offsetof(CompilationMessage, linePos) == offsetof(WGPUCompilationMessage, linePos),
        "offsetof mismatch for CompilationMessage::linePos");
static_assert(offsetof(CompilationMessage, offset) == offsetof(WGPUCompilationMessage, offset),
        "offsetof mismatch for CompilationMessage::offset");
static_assert(offsetof(CompilationMessage, length) == offsetof(WGPUCompilationMessage, length),
        "offsetof mismatch for CompilationMessage::length");

// ComputePassDescriptor implementation

ComputePassDescriptor::operator const WGPUComputePassDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPUComputePassDescriptor*>(this);
}

static_assert(sizeof(ComputePassDescriptor) == sizeof(WGPUComputePassDescriptor), "sizeof mismatch for ComputePassDescriptor");
static_assert(alignof(ComputePassDescriptor) == alignof(WGPUComputePassDescriptor), "alignof mismatch for ComputePassDescriptor");
static_assert(offsetof(ComputePassDescriptor, nextInChain) == offsetof(WGPUComputePassDescriptor, nextInChain),
        "offsetof mismatch for ComputePassDescriptor::nextInChain");
static_assert(offsetof(ComputePassDescriptor, label) == offsetof(WGPUComputePassDescriptor, label),
        "offsetof mismatch for ComputePassDescriptor::label");
static_assert(offsetof(ComputePassDescriptor, timestampWrites) == offsetof(WGPUComputePassDescriptor, timestampWrites),
        "offsetof mismatch for ComputePassDescriptor::timestampWrites");

// ComputeState implementation

ComputeState::operator const WGPUComputeState&() const noexcept {
    return *reinterpret_cast<const WGPUComputeState*>(this);
}

static_assert(sizeof(ComputeState) == sizeof(WGPUComputeState), "sizeof mismatch for ComputeState");
static_assert(alignof(ComputeState) == alignof(WGPUComputeState), "alignof mismatch for ComputeState");
static_assert(offsetof(ComputeState, nextInChain) == offsetof(WGPUComputeState, nextInChain),
        "offsetof mismatch for ComputeState::nextInChain");
static_assert(offsetof(ComputeState, module) == offsetof(WGPUComputeState, module),
        "offsetof mismatch for ComputeState::module");
static_assert(offsetof(ComputeState, entryPoint) == offsetof(WGPUComputeState, entryPoint),
        "offsetof mismatch for ComputeState::entryPoint");
static_assert(offsetof(ComputeState, constantCount) == offsetof(WGPUComputeState, constantCount),
        "offsetof mismatch for ComputeState::constantCount");
static_assert(offsetof(ComputeState, constants) == offsetof(WGPUComputeState, constants),
        "offsetof mismatch for ComputeState::constants");

// DepthStencilState implementation

DepthStencilState::operator const WGPUDepthStencilState&() const noexcept {
    return *reinterpret_cast<const WGPUDepthStencilState*>(this);
}

static_assert(sizeof(DepthStencilState) == sizeof(WGPUDepthStencilState), "sizeof mismatch for DepthStencilState");
static_assert(alignof(DepthStencilState) == alignof(WGPUDepthStencilState), "alignof mismatch for DepthStencilState");
static_assert(offsetof(DepthStencilState, nextInChain) == offsetof(WGPUDepthStencilState, nextInChain),
        "offsetof mismatch for DepthStencilState::nextInChain");
static_assert(offsetof(DepthStencilState, format) == offsetof(WGPUDepthStencilState, format),
        "offsetof mismatch for DepthStencilState::format");
static_assert(offsetof(DepthStencilState, depthWriteEnabled) == offsetof(WGPUDepthStencilState, depthWriteEnabled),
        "offsetof mismatch for DepthStencilState::depthWriteEnabled");
static_assert(offsetof(DepthStencilState, depthCompare) == offsetof(WGPUDepthStencilState, depthCompare),
        "offsetof mismatch for DepthStencilState::depthCompare");
static_assert(offsetof(DepthStencilState, stencilFront) == offsetof(WGPUDepthStencilState, stencilFront),
        "offsetof mismatch for DepthStencilState::stencilFront");
static_assert(offsetof(DepthStencilState, stencilBack) == offsetof(WGPUDepthStencilState, stencilBack),
        "offsetof mismatch for DepthStencilState::stencilBack");
static_assert(offsetof(DepthStencilState, stencilReadMask) == offsetof(WGPUDepthStencilState, stencilReadMask),
        "offsetof mismatch for DepthStencilState::stencilReadMask");
static_assert(offsetof(DepthStencilState, stencilWriteMask) == offsetof(WGPUDepthStencilState, stencilWriteMask),
        "offsetof mismatch for DepthStencilState::stencilWriteMask");
static_assert(offsetof(DepthStencilState, depthBias) == offsetof(WGPUDepthStencilState, depthBias),
        "offsetof mismatch for DepthStencilState::depthBias");
static_assert(offsetof(DepthStencilState, depthBiasSlopeScale) == offsetof(WGPUDepthStencilState, depthBiasSlopeScale),
        "offsetof mismatch for DepthStencilState::depthBiasSlopeScale");
static_assert(offsetof(DepthStencilState, depthBiasClamp) == offsetof(WGPUDepthStencilState, depthBiasClamp),
        "offsetof mismatch for DepthStencilState::depthBiasClamp");

// FutureWaitInfo implementation

FutureWaitInfo::operator const WGPUFutureWaitInfo&() const noexcept {
    return *reinterpret_cast<const WGPUFutureWaitInfo*>(this);
}

static_assert(sizeof(FutureWaitInfo) == sizeof(WGPUFutureWaitInfo), "sizeof mismatch for FutureWaitInfo");
static_assert(alignof(FutureWaitInfo) == alignof(WGPUFutureWaitInfo), "alignof mismatch for FutureWaitInfo");
static_assert(offsetof(FutureWaitInfo, future) == offsetof(WGPUFutureWaitInfo, future),
        "offsetof mismatch for FutureWaitInfo::future");
static_assert(offsetof(FutureWaitInfo, completed) == offsetof(WGPUFutureWaitInfo, completed),
        "offsetof mismatch for FutureWaitInfo::completed");

// InstanceDescriptor implementation

InstanceDescriptor::operator const WGPUInstanceDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPUInstanceDescriptor*>(this);
}

static_assert(sizeof(InstanceDescriptor) == sizeof(WGPUInstanceDescriptor), "sizeof mismatch for InstanceDescriptor");
static_assert(alignof(InstanceDescriptor) == alignof(WGPUInstanceDescriptor), "alignof mismatch for InstanceDescriptor");
static_assert(offsetof(InstanceDescriptor, nextInChain) == offsetof(WGPUInstanceDescriptor, nextInChain),
        "offsetof mismatch for InstanceDescriptor::nextInChain");
static_assert(offsetof(InstanceDescriptor, requiredFeatureCount) == offsetof(WGPUInstanceDescriptor, requiredFeatureCount),
        "offsetof mismatch for InstanceDescriptor::requiredFeatureCount");
static_assert(offsetof(InstanceDescriptor, requiredFeatures) == offsetof(WGPUInstanceDescriptor, requiredFeatures),
        "offsetof mismatch for InstanceDescriptor::requiredFeatures");
static_assert(offsetof(InstanceDescriptor, requiredLimits) == offsetof(WGPUInstanceDescriptor, requiredLimits),
        "offsetof mismatch for InstanceDescriptor::requiredLimits");

// Limits implementation

Limits::operator const WGPULimits&() const noexcept {
    return *reinterpret_cast<const WGPULimits*>(this);
}

static_assert(sizeof(Limits) == sizeof(WGPULimits), "sizeof mismatch for Limits");
static_assert(alignof(Limits) == alignof(WGPULimits), "alignof mismatch for Limits");
static_assert(offsetof(Limits, nextInChain) == offsetof(WGPULimits, nextInChain),
        "offsetof mismatch for Limits::nextInChain");
static_assert(offsetof(Limits, maxTextureDimension1D) == offsetof(WGPULimits, maxTextureDimension1D),
        "offsetof mismatch for Limits::maxTextureDimension1D");
static_assert(offsetof(Limits, maxTextureDimension2D) == offsetof(WGPULimits, maxTextureDimension2D),
        "offsetof mismatch for Limits::maxTextureDimension2D");
static_assert(offsetof(Limits, maxTextureDimension3D) == offsetof(WGPULimits, maxTextureDimension3D),
        "offsetof mismatch for Limits::maxTextureDimension3D");
static_assert(offsetof(Limits, maxTextureArrayLayers) == offsetof(WGPULimits, maxTextureArrayLayers),
        "offsetof mismatch for Limits::maxTextureArrayLayers");
static_assert(offsetof(Limits, maxBindGroups) == offsetof(WGPULimits, maxBindGroups),
        "offsetof mismatch for Limits::maxBindGroups");
static_assert(offsetof(Limits, maxBindGroupsPlusVertexBuffers) == offsetof(WGPULimits, maxBindGroupsPlusVertexBuffers),
        "offsetof mismatch for Limits::maxBindGroupsPlusVertexBuffers");
static_assert(offsetof(Limits, maxBindingsPerBindGroup) == offsetof(WGPULimits, maxBindingsPerBindGroup),
        "offsetof mismatch for Limits::maxBindingsPerBindGroup");
static_assert(offsetof(Limits, maxDynamicUniformBuffersPerPipelineLayout) == offsetof(WGPULimits, maxDynamicUniformBuffersPerPipelineLayout),
        "offsetof mismatch for Limits::maxDynamicUniformBuffersPerPipelineLayout");
static_assert(offsetof(Limits, maxDynamicStorageBuffersPerPipelineLayout) == offsetof(WGPULimits, maxDynamicStorageBuffersPerPipelineLayout),
        "offsetof mismatch for Limits::maxDynamicStorageBuffersPerPipelineLayout");
static_assert(offsetof(Limits, maxSampledTexturesPerShaderStage) == offsetof(WGPULimits, maxSampledTexturesPerShaderStage),
        "offsetof mismatch for Limits::maxSampledTexturesPerShaderStage");
static_assert(offsetof(Limits, maxSamplersPerShaderStage) == offsetof(WGPULimits, maxSamplersPerShaderStage),
        "offsetof mismatch for Limits::maxSamplersPerShaderStage");
static_assert(offsetof(Limits, maxStorageBuffersPerShaderStage) == offsetof(WGPULimits, maxStorageBuffersPerShaderStage),
        "offsetof mismatch for Limits::maxStorageBuffersPerShaderStage");
static_assert(offsetof(Limits, maxStorageTexturesPerShaderStage) == offsetof(WGPULimits, maxStorageTexturesPerShaderStage),
        "offsetof mismatch for Limits::maxStorageTexturesPerShaderStage");
static_assert(offsetof(Limits, maxUniformBuffersPerShaderStage) == offsetof(WGPULimits, maxUniformBuffersPerShaderStage),
        "offsetof mismatch for Limits::maxUniformBuffersPerShaderStage");
static_assert(offsetof(Limits, maxUniformBufferBindingSize) == offsetof(WGPULimits, maxUniformBufferBindingSize),
        "offsetof mismatch for Limits::maxUniformBufferBindingSize");
static_assert(offsetof(Limits, maxStorageBufferBindingSize) == offsetof(WGPULimits, maxStorageBufferBindingSize),
        "offsetof mismatch for Limits::maxStorageBufferBindingSize");
static_assert(offsetof(Limits, minUniformBufferOffsetAlignment) == offsetof(WGPULimits, minUniformBufferOffsetAlignment),
        "offsetof mismatch for Limits::minUniformBufferOffsetAlignment");
static_assert(offsetof(Limits, minStorageBufferOffsetAlignment) == offsetof(WGPULimits, minStorageBufferOffsetAlignment),
        "offsetof mismatch for Limits::minStorageBufferOffsetAlignment");
static_assert(offsetof(Limits, maxVertexBuffers) == offsetof(WGPULimits, maxVertexBuffers),
        "offsetof mismatch for Limits::maxVertexBuffers");
static_assert(offsetof(Limits, maxBufferSize) == offsetof(WGPULimits, maxBufferSize),
        "offsetof mismatch for Limits::maxBufferSize");
static_assert(offsetof(Limits, maxVertexAttributes) == offsetof(WGPULimits, maxVertexAttributes),
        "offsetof mismatch for Limits::maxVertexAttributes");
static_assert(offsetof(Limits, maxVertexBufferArrayStride) == offsetof(WGPULimits, maxVertexBufferArrayStride),
        "offsetof mismatch for Limits::maxVertexBufferArrayStride");
static_assert(offsetof(Limits, maxInterStageShaderVariables) == offsetof(WGPULimits, maxInterStageShaderVariables),
        "offsetof mismatch for Limits::maxInterStageShaderVariables");
static_assert(offsetof(Limits, maxColorAttachments) == offsetof(WGPULimits, maxColorAttachments),
        "offsetof mismatch for Limits::maxColorAttachments");
static_assert(offsetof(Limits, maxColorAttachmentBytesPerSample) == offsetof(WGPULimits, maxColorAttachmentBytesPerSample),
        "offsetof mismatch for Limits::maxColorAttachmentBytesPerSample");
static_assert(offsetof(Limits, maxComputeWorkgroupStorageSize) == offsetof(WGPULimits, maxComputeWorkgroupStorageSize),
        "offsetof mismatch for Limits::maxComputeWorkgroupStorageSize");
static_assert(offsetof(Limits, maxComputeInvocationsPerWorkgroup) == offsetof(WGPULimits, maxComputeInvocationsPerWorkgroup),
        "offsetof mismatch for Limits::maxComputeInvocationsPerWorkgroup");
static_assert(offsetof(Limits, maxComputeWorkgroupSizeX) == offsetof(WGPULimits, maxComputeWorkgroupSizeX),
        "offsetof mismatch for Limits::maxComputeWorkgroupSizeX");
static_assert(offsetof(Limits, maxComputeWorkgroupSizeY) == offsetof(WGPULimits, maxComputeWorkgroupSizeY),
        "offsetof mismatch for Limits::maxComputeWorkgroupSizeY");
static_assert(offsetof(Limits, maxComputeWorkgroupSizeZ) == offsetof(WGPULimits, maxComputeWorkgroupSizeZ),
        "offsetof mismatch for Limits::maxComputeWorkgroupSizeZ");
static_assert(offsetof(Limits, maxComputeWorkgroupsPerDimension) == offsetof(WGPULimits, maxComputeWorkgroupsPerDimension),
        "offsetof mismatch for Limits::maxComputeWorkgroupsPerDimension");
static_assert(offsetof(Limits, maxImmediateSize) == offsetof(WGPULimits, maxImmediateSize),
        "offsetof mismatch for Limits::maxImmediateSize");

// RenderPassColorAttachment implementation

RenderPassColorAttachment::operator const WGPURenderPassColorAttachment&() const noexcept {
    return *reinterpret_cast<const WGPURenderPassColorAttachment*>(this);
}

static_assert(sizeof(RenderPassColorAttachment) == sizeof(WGPURenderPassColorAttachment), "sizeof mismatch for RenderPassColorAttachment");
static_assert(alignof(RenderPassColorAttachment) == alignof(WGPURenderPassColorAttachment), "alignof mismatch for RenderPassColorAttachment");
static_assert(offsetof(RenderPassColorAttachment, nextInChain) == offsetof(WGPURenderPassColorAttachment, nextInChain),
        "offsetof mismatch for RenderPassColorAttachment::nextInChain");
static_assert(offsetof(RenderPassColorAttachment, view) == offsetof(WGPURenderPassColorAttachment, view),
        "offsetof mismatch for RenderPassColorAttachment::view");
static_assert(offsetof(RenderPassColorAttachment, depthSlice) == offsetof(WGPURenderPassColorAttachment, depthSlice),
        "offsetof mismatch for RenderPassColorAttachment::depthSlice");
static_assert(offsetof(RenderPassColorAttachment, resolveTarget) == offsetof(WGPURenderPassColorAttachment, resolveTarget),
        "offsetof mismatch for RenderPassColorAttachment::resolveTarget");
static_assert(offsetof(RenderPassColorAttachment, loadOp) == offsetof(WGPURenderPassColorAttachment, loadOp),
        "offsetof mismatch for RenderPassColorAttachment::loadOp");
static_assert(offsetof(RenderPassColorAttachment, storeOp) == offsetof(WGPURenderPassColorAttachment, storeOp),
        "offsetof mismatch for RenderPassColorAttachment::storeOp");
static_assert(offsetof(RenderPassColorAttachment, clearValue) == offsetof(WGPURenderPassColorAttachment, clearValue),
        "offsetof mismatch for RenderPassColorAttachment::clearValue");

// RequestAdapterOptions implementation

RequestAdapterOptions::operator const WGPURequestAdapterOptions&() const noexcept {
    return *reinterpret_cast<const WGPURequestAdapterOptions*>(this);
}

static_assert(sizeof(RequestAdapterOptions) == sizeof(WGPURequestAdapterOptions), "sizeof mismatch for RequestAdapterOptions");
static_assert(alignof(RequestAdapterOptions) == alignof(WGPURequestAdapterOptions), "alignof mismatch for RequestAdapterOptions");
static_assert(offsetof(RequestAdapterOptions, nextInChain) == offsetof(WGPURequestAdapterOptions, nextInChain),
        "offsetof mismatch for RequestAdapterOptions::nextInChain");
static_assert(offsetof(RequestAdapterOptions, featureLevel) == offsetof(WGPURequestAdapterOptions, featureLevel),
        "offsetof mismatch for RequestAdapterOptions::featureLevel");
static_assert(offsetof(RequestAdapterOptions, powerPreference) == offsetof(WGPURequestAdapterOptions, powerPreference),
        "offsetof mismatch for RequestAdapterOptions::powerPreference");
static_assert(offsetof(RequestAdapterOptions, forceFallbackAdapter) == offsetof(WGPURequestAdapterOptions, forceFallbackAdapter),
        "offsetof mismatch for RequestAdapterOptions::forceFallbackAdapter");
static_assert(offsetof(RequestAdapterOptions, backendType) == offsetof(WGPURequestAdapterOptions, backendType),
        "offsetof mismatch for RequestAdapterOptions::backendType");
static_assert(offsetof(RequestAdapterOptions, compatibleSurface) == offsetof(WGPURequestAdapterOptions, compatibleSurface),
        "offsetof mismatch for RequestAdapterOptions::compatibleSurface");

// ShaderModuleDescriptor implementation

ShaderModuleDescriptor::operator const WGPUShaderModuleDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPUShaderModuleDescriptor*>(this);
}

static_assert(sizeof(ShaderModuleDescriptor) == sizeof(WGPUShaderModuleDescriptor), "sizeof mismatch for ShaderModuleDescriptor");
static_assert(alignof(ShaderModuleDescriptor) == alignof(WGPUShaderModuleDescriptor), "alignof mismatch for ShaderModuleDescriptor");
static_assert(offsetof(ShaderModuleDescriptor, nextInChain) == offsetof(WGPUShaderModuleDescriptor, nextInChain),
        "offsetof mismatch for ShaderModuleDescriptor::nextInChain");
static_assert(offsetof(ShaderModuleDescriptor, label) == offsetof(WGPUShaderModuleDescriptor, label),
        "offsetof mismatch for ShaderModuleDescriptor::label");

// SurfaceDescriptor implementation

SurfaceDescriptor::operator const WGPUSurfaceDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPUSurfaceDescriptor*>(this);
}

static_assert(sizeof(SurfaceDescriptor) == sizeof(WGPUSurfaceDescriptor), "sizeof mismatch for SurfaceDescriptor");
static_assert(alignof(SurfaceDescriptor) == alignof(WGPUSurfaceDescriptor), "alignof mismatch for SurfaceDescriptor");
static_assert(offsetof(SurfaceDescriptor, nextInChain) == offsetof(WGPUSurfaceDescriptor, nextInChain),
        "offsetof mismatch for SurfaceDescriptor::nextInChain");
static_assert(offsetof(SurfaceDescriptor, label) == offsetof(WGPUSurfaceDescriptor, label),
        "offsetof mismatch for SurfaceDescriptor::label");

// TexelCopyBufferInfo implementation

TexelCopyBufferInfo::operator const WGPUTexelCopyBufferInfo&() const noexcept {
    return *reinterpret_cast<const WGPUTexelCopyBufferInfo*>(this);
}

static_assert(sizeof(TexelCopyBufferInfo) == sizeof(WGPUTexelCopyBufferInfo), "sizeof mismatch for TexelCopyBufferInfo");
static_assert(alignof(TexelCopyBufferInfo) == alignof(WGPUTexelCopyBufferInfo), "alignof mismatch for TexelCopyBufferInfo");
static_assert(offsetof(TexelCopyBufferInfo, layout) == offsetof(WGPUTexelCopyBufferInfo, layout),
        "offsetof mismatch for TexelCopyBufferInfo::layout");
static_assert(offsetof(TexelCopyBufferInfo, buffer) == offsetof(WGPUTexelCopyBufferInfo, buffer),
        "offsetof mismatch for TexelCopyBufferInfo::buffer");

// TexelCopyTextureInfo implementation

TexelCopyTextureInfo::operator const WGPUTexelCopyTextureInfo&() const noexcept {
    return *reinterpret_cast<const WGPUTexelCopyTextureInfo*>(this);
}

static_assert(sizeof(TexelCopyTextureInfo) == sizeof(WGPUTexelCopyTextureInfo), "sizeof mismatch for TexelCopyTextureInfo");
static_assert(alignof(TexelCopyTextureInfo) == alignof(WGPUTexelCopyTextureInfo), "alignof mismatch for TexelCopyTextureInfo");
static_assert(offsetof(TexelCopyTextureInfo, texture) == offsetof(WGPUTexelCopyTextureInfo, texture),
        "offsetof mismatch for TexelCopyTextureInfo::texture");
static_assert(offsetof(TexelCopyTextureInfo, mipLevel) == offsetof(WGPUTexelCopyTextureInfo, mipLevel),
        "offsetof mismatch for TexelCopyTextureInfo::mipLevel");
static_assert(offsetof(TexelCopyTextureInfo, origin) == offsetof(WGPUTexelCopyTextureInfo, origin),
        "offsetof mismatch for TexelCopyTextureInfo::origin");
static_assert(offsetof(TexelCopyTextureInfo, aspect) == offsetof(WGPUTexelCopyTextureInfo, aspect),
        "offsetof mismatch for TexelCopyTextureInfo::aspect");

// TextureComponentSwizzleDescriptor implementation
TextureComponentSwizzleDescriptor::TextureComponentSwizzleDescriptor()
  : ChainedStruct { nullptr, SType::TextureComponentSwizzleDescriptor } {}
struct TextureComponentSwizzleDescriptor::Init {
    ChainedStruct * const nextInChain;
    TextureComponentSwizzle swizzle = {};
};
TextureComponentSwizzleDescriptor::TextureComponentSwizzleDescriptor(TextureComponentSwizzleDescriptor::Init&& init)
  : ChainedStruct { init.nextInChain, SType::TextureComponentSwizzleDescriptor }, 
    swizzle(std::move(init.swizzle)){}

TextureComponentSwizzleDescriptor::operator const WGPUTextureComponentSwizzleDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPUTextureComponentSwizzleDescriptor*>(this);
}

static_assert(sizeof(TextureComponentSwizzleDescriptor) == sizeof(WGPUTextureComponentSwizzleDescriptor), "sizeof mismatch for TextureComponentSwizzleDescriptor");
static_assert(alignof(TextureComponentSwizzleDescriptor) == alignof(WGPUTextureComponentSwizzleDescriptor), "alignof mismatch for TextureComponentSwizzleDescriptor");
static_assert(offsetof(TextureComponentSwizzleDescriptor, swizzle) == offsetof(WGPUTextureComponentSwizzleDescriptor, swizzle),
        "offsetof mismatch for TextureComponentSwizzleDescriptor::swizzle");

// TextureDescriptor implementation

TextureDescriptor::operator const WGPUTextureDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPUTextureDescriptor*>(this);
}

static_assert(sizeof(TextureDescriptor) == sizeof(WGPUTextureDescriptor), "sizeof mismatch for TextureDescriptor");
static_assert(alignof(TextureDescriptor) == alignof(WGPUTextureDescriptor), "alignof mismatch for TextureDescriptor");
static_assert(offsetof(TextureDescriptor, nextInChain) == offsetof(WGPUTextureDescriptor, nextInChain),
        "offsetof mismatch for TextureDescriptor::nextInChain");
static_assert(offsetof(TextureDescriptor, label) == offsetof(WGPUTextureDescriptor, label),
        "offsetof mismatch for TextureDescriptor::label");
static_assert(offsetof(TextureDescriptor, usage) == offsetof(WGPUTextureDescriptor, usage),
        "offsetof mismatch for TextureDescriptor::usage");
static_assert(offsetof(TextureDescriptor, dimension) == offsetof(WGPUTextureDescriptor, dimension),
        "offsetof mismatch for TextureDescriptor::dimension");
static_assert(offsetof(TextureDescriptor, size) == offsetof(WGPUTextureDescriptor, size),
        "offsetof mismatch for TextureDescriptor::size");
static_assert(offsetof(TextureDescriptor, format) == offsetof(WGPUTextureDescriptor, format),
        "offsetof mismatch for TextureDescriptor::format");
static_assert(offsetof(TextureDescriptor, mipLevelCount) == offsetof(WGPUTextureDescriptor, mipLevelCount),
        "offsetof mismatch for TextureDescriptor::mipLevelCount");
static_assert(offsetof(TextureDescriptor, sampleCount) == offsetof(WGPUTextureDescriptor, sampleCount),
        "offsetof mismatch for TextureDescriptor::sampleCount");
static_assert(offsetof(TextureDescriptor, viewFormatCount) == offsetof(WGPUTextureDescriptor, viewFormatCount),
        "offsetof mismatch for TextureDescriptor::viewFormatCount");
static_assert(offsetof(TextureDescriptor, viewFormats) == offsetof(WGPUTextureDescriptor, viewFormats),
        "offsetof mismatch for TextureDescriptor::viewFormats");

// VertexBufferLayout implementation

VertexBufferLayout::operator const WGPUVertexBufferLayout&() const noexcept {
    return *reinterpret_cast<const WGPUVertexBufferLayout*>(this);
}

static_assert(sizeof(VertexBufferLayout) == sizeof(WGPUVertexBufferLayout), "sizeof mismatch for VertexBufferLayout");
static_assert(alignof(VertexBufferLayout) == alignof(WGPUVertexBufferLayout), "alignof mismatch for VertexBufferLayout");
static_assert(offsetof(VertexBufferLayout, nextInChain) == offsetof(WGPUVertexBufferLayout, nextInChain),
        "offsetof mismatch for VertexBufferLayout::nextInChain");
static_assert(offsetof(VertexBufferLayout, stepMode) == offsetof(WGPUVertexBufferLayout, stepMode),
        "offsetof mismatch for VertexBufferLayout::stepMode");
static_assert(offsetof(VertexBufferLayout, arrayStride) == offsetof(WGPUVertexBufferLayout, arrayStride),
        "offsetof mismatch for VertexBufferLayout::arrayStride");
static_assert(offsetof(VertexBufferLayout, attributeCount) == offsetof(WGPUVertexBufferLayout, attributeCount),
        "offsetof mismatch for VertexBufferLayout::attributeCount");
static_assert(offsetof(VertexBufferLayout, attributes) == offsetof(WGPUVertexBufferLayout, attributes),
        "offsetof mismatch for VertexBufferLayout::attributes");

// BindGroupLayoutDescriptor implementation

BindGroupLayoutDescriptor::operator const WGPUBindGroupLayoutDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPUBindGroupLayoutDescriptor*>(this);
}

static_assert(sizeof(BindGroupLayoutDescriptor) == sizeof(WGPUBindGroupLayoutDescriptor), "sizeof mismatch for BindGroupLayoutDescriptor");
static_assert(alignof(BindGroupLayoutDescriptor) == alignof(WGPUBindGroupLayoutDescriptor), "alignof mismatch for BindGroupLayoutDescriptor");
static_assert(offsetof(BindGroupLayoutDescriptor, nextInChain) == offsetof(WGPUBindGroupLayoutDescriptor, nextInChain),
        "offsetof mismatch for BindGroupLayoutDescriptor::nextInChain");
static_assert(offsetof(BindGroupLayoutDescriptor, label) == offsetof(WGPUBindGroupLayoutDescriptor, label),
        "offsetof mismatch for BindGroupLayoutDescriptor::label");
static_assert(offsetof(BindGroupLayoutDescriptor, entryCount) == offsetof(WGPUBindGroupLayoutDescriptor, entryCount),
        "offsetof mismatch for BindGroupLayoutDescriptor::entryCount");
static_assert(offsetof(BindGroupLayoutDescriptor, entries) == offsetof(WGPUBindGroupLayoutDescriptor, entries),
        "offsetof mismatch for BindGroupLayoutDescriptor::entries");

// ColorTargetState implementation

ColorTargetState::operator const WGPUColorTargetState&() const noexcept {
    return *reinterpret_cast<const WGPUColorTargetState*>(this);
}

static_assert(sizeof(ColorTargetState) == sizeof(WGPUColorTargetState), "sizeof mismatch for ColorTargetState");
static_assert(alignof(ColorTargetState) == alignof(WGPUColorTargetState), "alignof mismatch for ColorTargetState");
static_assert(offsetof(ColorTargetState, nextInChain) == offsetof(WGPUColorTargetState, nextInChain),
        "offsetof mismatch for ColorTargetState::nextInChain");
static_assert(offsetof(ColorTargetState, format) == offsetof(WGPUColorTargetState, format),
        "offsetof mismatch for ColorTargetState::format");
static_assert(offsetof(ColorTargetState, blend) == offsetof(WGPUColorTargetState, blend),
        "offsetof mismatch for ColorTargetState::blend");
static_assert(offsetof(ColorTargetState, writeMask) == offsetof(WGPUColorTargetState, writeMask),
        "offsetof mismatch for ColorTargetState::writeMask");

// CompilationInfo implementation

CompilationInfo::operator const WGPUCompilationInfo&() const noexcept {
    return *reinterpret_cast<const WGPUCompilationInfo*>(this);
}

static_assert(sizeof(CompilationInfo) == sizeof(WGPUCompilationInfo), "sizeof mismatch for CompilationInfo");
static_assert(alignof(CompilationInfo) == alignof(WGPUCompilationInfo), "alignof mismatch for CompilationInfo");
static_assert(offsetof(CompilationInfo, nextInChain) == offsetof(WGPUCompilationInfo, nextInChain),
        "offsetof mismatch for CompilationInfo::nextInChain");
static_assert(offsetof(CompilationInfo, messageCount) == offsetof(WGPUCompilationInfo, messageCount),
        "offsetof mismatch for CompilationInfo::messageCount");
static_assert(offsetof(CompilationInfo, messages) == offsetof(WGPUCompilationInfo, messages),
        "offsetof mismatch for CompilationInfo::messages");

// ComputePipelineDescriptor implementation

ComputePipelineDescriptor::operator const WGPUComputePipelineDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPUComputePipelineDescriptor*>(this);
}

static_assert(sizeof(ComputePipelineDescriptor) == sizeof(WGPUComputePipelineDescriptor), "sizeof mismatch for ComputePipelineDescriptor");
static_assert(alignof(ComputePipelineDescriptor) == alignof(WGPUComputePipelineDescriptor), "alignof mismatch for ComputePipelineDescriptor");
static_assert(offsetof(ComputePipelineDescriptor, nextInChain) == offsetof(WGPUComputePipelineDescriptor, nextInChain),
        "offsetof mismatch for ComputePipelineDescriptor::nextInChain");
static_assert(offsetof(ComputePipelineDescriptor, label) == offsetof(WGPUComputePipelineDescriptor, label),
        "offsetof mismatch for ComputePipelineDescriptor::label");
static_assert(offsetof(ComputePipelineDescriptor, layout) == offsetof(WGPUComputePipelineDescriptor, layout),
        "offsetof mismatch for ComputePipelineDescriptor::layout");
static_assert(offsetof(ComputePipelineDescriptor, compute) == offsetof(WGPUComputePipelineDescriptor, compute),
        "offsetof mismatch for ComputePipelineDescriptor::compute");

// RenderPassDescriptor implementation

RenderPassDescriptor::operator const WGPURenderPassDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPURenderPassDescriptor*>(this);
}

static_assert(sizeof(RenderPassDescriptor) == sizeof(WGPURenderPassDescriptor), "sizeof mismatch for RenderPassDescriptor");
static_assert(alignof(RenderPassDescriptor) == alignof(WGPURenderPassDescriptor), "alignof mismatch for RenderPassDescriptor");
static_assert(offsetof(RenderPassDescriptor, nextInChain) == offsetof(WGPURenderPassDescriptor, nextInChain),
        "offsetof mismatch for RenderPassDescriptor::nextInChain");
static_assert(offsetof(RenderPassDescriptor, label) == offsetof(WGPURenderPassDescriptor, label),
        "offsetof mismatch for RenderPassDescriptor::label");
static_assert(offsetof(RenderPassDescriptor, colorAttachmentCount) == offsetof(WGPURenderPassDescriptor, colorAttachmentCount),
        "offsetof mismatch for RenderPassDescriptor::colorAttachmentCount");
static_assert(offsetof(RenderPassDescriptor, colorAttachments) == offsetof(WGPURenderPassDescriptor, colorAttachments),
        "offsetof mismatch for RenderPassDescriptor::colorAttachments");
static_assert(offsetof(RenderPassDescriptor, depthStencilAttachment) == offsetof(WGPURenderPassDescriptor, depthStencilAttachment),
        "offsetof mismatch for RenderPassDescriptor::depthStencilAttachment");
static_assert(offsetof(RenderPassDescriptor, occlusionQuerySet) == offsetof(WGPURenderPassDescriptor, occlusionQuerySet),
        "offsetof mismatch for RenderPassDescriptor::occlusionQuerySet");
static_assert(offsetof(RenderPassDescriptor, timestampWrites) == offsetof(WGPURenderPassDescriptor, timestampWrites),
        "offsetof mismatch for RenderPassDescriptor::timestampWrites");

// TextureViewDescriptor implementation

TextureViewDescriptor::operator const WGPUTextureViewDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPUTextureViewDescriptor*>(this);
}

static_assert(sizeof(TextureViewDescriptor) == sizeof(WGPUTextureViewDescriptor), "sizeof mismatch for TextureViewDescriptor");
static_assert(alignof(TextureViewDescriptor) == alignof(WGPUTextureViewDescriptor), "alignof mismatch for TextureViewDescriptor");
static_assert(offsetof(TextureViewDescriptor, nextInChain) == offsetof(WGPUTextureViewDescriptor, nextInChain),
        "offsetof mismatch for TextureViewDescriptor::nextInChain");
static_assert(offsetof(TextureViewDescriptor, label) == offsetof(WGPUTextureViewDescriptor, label),
        "offsetof mismatch for TextureViewDescriptor::label");
static_assert(offsetof(TextureViewDescriptor, format) == offsetof(WGPUTextureViewDescriptor, format),
        "offsetof mismatch for TextureViewDescriptor::format");
static_assert(offsetof(TextureViewDescriptor, dimension) == offsetof(WGPUTextureViewDescriptor, dimension),
        "offsetof mismatch for TextureViewDescriptor::dimension");
static_assert(offsetof(TextureViewDescriptor, baseMipLevel) == offsetof(WGPUTextureViewDescriptor, baseMipLevel),
        "offsetof mismatch for TextureViewDescriptor::baseMipLevel");
static_assert(offsetof(TextureViewDescriptor, mipLevelCount) == offsetof(WGPUTextureViewDescriptor, mipLevelCount),
        "offsetof mismatch for TextureViewDescriptor::mipLevelCount");
static_assert(offsetof(TextureViewDescriptor, baseArrayLayer) == offsetof(WGPUTextureViewDescriptor, baseArrayLayer),
        "offsetof mismatch for TextureViewDescriptor::baseArrayLayer");
static_assert(offsetof(TextureViewDescriptor, arrayLayerCount) == offsetof(WGPUTextureViewDescriptor, arrayLayerCount),
        "offsetof mismatch for TextureViewDescriptor::arrayLayerCount");
static_assert(offsetof(TextureViewDescriptor, aspect) == offsetof(WGPUTextureViewDescriptor, aspect),
        "offsetof mismatch for TextureViewDescriptor::aspect");
static_assert(offsetof(TextureViewDescriptor, usage) == offsetof(WGPUTextureViewDescriptor, usage),
        "offsetof mismatch for TextureViewDescriptor::usage");

// VertexState implementation

VertexState::operator const WGPUVertexState&() const noexcept {
    return *reinterpret_cast<const WGPUVertexState*>(this);
}

static_assert(sizeof(VertexState) == sizeof(WGPUVertexState), "sizeof mismatch for VertexState");
static_assert(alignof(VertexState) == alignof(WGPUVertexState), "alignof mismatch for VertexState");
static_assert(offsetof(VertexState, nextInChain) == offsetof(WGPUVertexState, nextInChain),
        "offsetof mismatch for VertexState::nextInChain");
static_assert(offsetof(VertexState, module) == offsetof(WGPUVertexState, module),
        "offsetof mismatch for VertexState::module");
static_assert(offsetof(VertexState, entryPoint) == offsetof(WGPUVertexState, entryPoint),
        "offsetof mismatch for VertexState::entryPoint");
static_assert(offsetof(VertexState, constantCount) == offsetof(WGPUVertexState, constantCount),
        "offsetof mismatch for VertexState::constantCount");
static_assert(offsetof(VertexState, constants) == offsetof(WGPUVertexState, constants),
        "offsetof mismatch for VertexState::constants");
static_assert(offsetof(VertexState, bufferCount) == offsetof(WGPUVertexState, bufferCount),
        "offsetof mismatch for VertexState::bufferCount");
static_assert(offsetof(VertexState, buffers) == offsetof(WGPUVertexState, buffers),
        "offsetof mismatch for VertexState::buffers");

// FragmentState implementation

FragmentState::operator const WGPUFragmentState&() const noexcept {
    return *reinterpret_cast<const WGPUFragmentState*>(this);
}

static_assert(sizeof(FragmentState) == sizeof(WGPUFragmentState), "sizeof mismatch for FragmentState");
static_assert(alignof(FragmentState) == alignof(WGPUFragmentState), "alignof mismatch for FragmentState");
static_assert(offsetof(FragmentState, nextInChain) == offsetof(WGPUFragmentState, nextInChain),
        "offsetof mismatch for FragmentState::nextInChain");
static_assert(offsetof(FragmentState, module) == offsetof(WGPUFragmentState, module),
        "offsetof mismatch for FragmentState::module");
static_assert(offsetof(FragmentState, entryPoint) == offsetof(WGPUFragmentState, entryPoint),
        "offsetof mismatch for FragmentState::entryPoint");
static_assert(offsetof(FragmentState, constantCount) == offsetof(WGPUFragmentState, constantCount),
        "offsetof mismatch for FragmentState::constantCount");
static_assert(offsetof(FragmentState, constants) == offsetof(WGPUFragmentState, constants),
        "offsetof mismatch for FragmentState::constants");
static_assert(offsetof(FragmentState, targetCount) == offsetof(WGPUFragmentState, targetCount),
        "offsetof mismatch for FragmentState::targetCount");
static_assert(offsetof(FragmentState, targets) == offsetof(WGPUFragmentState, targets),
        "offsetof mismatch for FragmentState::targets");

// RenderPipelineDescriptor implementation

RenderPipelineDescriptor::operator const WGPURenderPipelineDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPURenderPipelineDescriptor*>(this);
}

static_assert(sizeof(RenderPipelineDescriptor) == sizeof(WGPURenderPipelineDescriptor), "sizeof mismatch for RenderPipelineDescriptor");
static_assert(alignof(RenderPipelineDescriptor) == alignof(WGPURenderPipelineDescriptor), "alignof mismatch for RenderPipelineDescriptor");
static_assert(offsetof(RenderPipelineDescriptor, nextInChain) == offsetof(WGPURenderPipelineDescriptor, nextInChain),
        "offsetof mismatch for RenderPipelineDescriptor::nextInChain");
static_assert(offsetof(RenderPipelineDescriptor, label) == offsetof(WGPURenderPipelineDescriptor, label),
        "offsetof mismatch for RenderPipelineDescriptor::label");
static_assert(offsetof(RenderPipelineDescriptor, layout) == offsetof(WGPURenderPipelineDescriptor, layout),
        "offsetof mismatch for RenderPipelineDescriptor::layout");
static_assert(offsetof(RenderPipelineDescriptor, vertex) == offsetof(WGPURenderPipelineDescriptor, vertex),
        "offsetof mismatch for RenderPipelineDescriptor::vertex");
static_assert(offsetof(RenderPipelineDescriptor, primitive) == offsetof(WGPURenderPipelineDescriptor, primitive),
        "offsetof mismatch for RenderPipelineDescriptor::primitive");
static_assert(offsetof(RenderPipelineDescriptor, depthStencil) == offsetof(WGPURenderPipelineDescriptor, depthStencil),
        "offsetof mismatch for RenderPipelineDescriptor::depthStencil");
static_assert(offsetof(RenderPipelineDescriptor, multisample) == offsetof(WGPURenderPipelineDescriptor, multisample),
        "offsetof mismatch for RenderPipelineDescriptor::multisample");
static_assert(offsetof(RenderPipelineDescriptor, fragment) == offsetof(WGPURenderPipelineDescriptor, fragment),
        "offsetof mismatch for RenderPipelineDescriptor::fragment");

// DeviceDescriptor implementation

DeviceDescriptor::operator const WGPUDeviceDescriptor&() const noexcept {
    return *reinterpret_cast<const WGPUDeviceDescriptor*>(this);
}

DeviceDescriptor::DeviceDescriptor() : detail::DeviceDescriptor {} {
    static_assert(offsetof(DeviceDescriptor, nextInChain) == offsetof(WGPUDeviceDescriptor, nextInChain),
                "offsetof mismatch for DeviceDescriptor::nextInChain");
    static_assert(offsetof(DeviceDescriptor, label) == offsetof(WGPUDeviceDescriptor, label),
            "offsetof mismatch for DeviceDescriptor::label");
    static_assert(offsetof(DeviceDescriptor, requiredFeatureCount) == offsetof(WGPUDeviceDescriptor, requiredFeatureCount),
            "offsetof mismatch for DeviceDescriptor::requiredFeatureCount");
    static_assert(offsetof(DeviceDescriptor, requiredFeatures) == offsetof(WGPUDeviceDescriptor, requiredFeatures),
            "offsetof mismatch for DeviceDescriptor::requiredFeatures");
    static_assert(offsetof(DeviceDescriptor, requiredLimits) == offsetof(WGPUDeviceDescriptor, requiredLimits),
            "offsetof mismatch for DeviceDescriptor::requiredLimits");
    static_assert(offsetof(DeviceDescriptor, defaultQueue) == offsetof(WGPUDeviceDescriptor, defaultQueue),
            "offsetof mismatch for DeviceDescriptor::defaultQueue");
    static_assert(offsetof(DeviceDescriptor, deviceLostCallbackInfo) == offsetof(WGPUDeviceDescriptor, deviceLostCallbackInfo),
            "offsetof mismatch for DeviceDescriptor::deviceLostCallbackInfo");
    static_assert(offsetof(DeviceDescriptor, uncapturedErrorCallbackInfo) == offsetof(WGPUDeviceDescriptor, uncapturedErrorCallbackInfo),
            "offsetof mismatch for DeviceDescriptor::uncapturedErrorCallbackInfo");
}

struct DeviceDescriptor::Init {
    ChainedStruct const * nextInChain;
    StringView label = {};
    size_t requiredFeatureCount = 0;
    FeatureName const * requiredFeatures = nullptr;
    Limits const * requiredLimits = nullptr;
    QueueDescriptor defaultQueue = {};
};

DeviceDescriptor::DeviceDescriptor(DeviceDescriptor::Init&& init) : detail::DeviceDescriptor {
    init.nextInChain, 
    std::move(init.label), 
    std::move(init.requiredFeatureCount), 
    std::move(init.requiredFeatures), 
    std::move(init.requiredLimits), 
    std::move(init.defaultQueue)} {}

static_assert(sizeof(DeviceDescriptor) == sizeof(WGPUDeviceDescriptor), "sizeof mismatch for DeviceDescriptor");
static_assert(alignof(DeviceDescriptor) == alignof(WGPUDeviceDescriptor), "alignof mismatch for DeviceDescriptor");

template <typename F, typename T, typename Cb, typename>
void DeviceDescriptor::SetDeviceLostCallback(CallbackMode callbackMode, F callback, T userdata) {
    assert(deviceLostCallbackInfo.callback == nullptr);

    deviceLostCallbackInfo.mode = static_cast<WGPUCallbackMode>(callbackMode);
    deviceLostCallbackInfo.callback = [](WGPUDevice const * device, WGPUDeviceLostReason reason, WGPUStringView message, void* callback_param, void* userdata_param) {
        auto cb = reinterpret_cast<Cb*>(callback_param);
        // We manually acquire and release the device to avoid changing any ref counts.
        auto apiDevice = Device::Acquire(*device);
        (*cb)(apiDevice, static_cast<DeviceLostReason>(reason), message, static_cast<T>(userdata_param));
        apiDevice.MoveToCHandle();
    };
    deviceLostCallbackInfo.userdata1 = reinterpret_cast<void*>(+callback);
    deviceLostCallbackInfo.userdata2 = reinterpret_cast<void*>(userdata);
}

template <typename L, typename Cb, typename>
void DeviceDescriptor::SetDeviceLostCallback(CallbackMode callbackMode, L callback) {
    assert(deviceLostCallbackInfo.callback == nullptr);
    using F = DeviceLostCallback<void>;

    deviceLostCallbackInfo.mode = static_cast<WGPUCallbackMode>(callbackMode);
    if constexpr (std::is_convertible_v<L, F*>) {
        deviceLostCallbackInfo.callback = [](WGPUDevice const * device, WGPUDeviceLostReason reason, WGPUStringView message, void* callback_param, void*) {
            auto cb = reinterpret_cast<F*>(callback_param);
            // We manually acquire and release the device to avoid changing any ref counts.
            auto apiDevice = Device::Acquire(*device);
            (*cb)(apiDevice, static_cast<DeviceLostReason>(reason), message);
            apiDevice.MoveToCHandle();
        };
        deviceLostCallbackInfo.userdata1 = reinterpret_cast<void*>(+callback);
        deviceLostCallbackInfo.userdata2 = nullptr;
    } else {
        auto* lambda = new L(std::move(callback));
        deviceLostCallbackInfo.callback = [](WGPUDevice const * device, WGPUDeviceLostReason reason, WGPUStringView message, void* callback_param, void*) {
            std::unique_ptr<L> the_lambda(reinterpret_cast<L*>(callback_param));
            // We manually acquire and release the device to avoid changing any ref counts.
            auto apiDevice = Device::Acquire(*device);
            (*the_lambda)(apiDevice, static_cast<DeviceLostReason>(reason), message);
            apiDevice.MoveToCHandle();
        };
        deviceLostCallbackInfo.userdata1 = reinterpret_cast<void*>(lambda);
        deviceLostCallbackInfo.userdata2 = nullptr;
    }
}

template <typename F, typename T, typename Cb, typename>
void DeviceDescriptor::SetUncapturedErrorCallback(F callback, T userdata) {
    assert(uncapturedErrorCallbackInfo.callback == nullptr);

    uncapturedErrorCallbackInfo.callback = [](WGPUDevice const * device, WGPUErrorType type, WGPUStringView message, void* callback_param, void* userdata_param) {
        auto cb = reinterpret_cast<Cb*>(callback_param);
        // We manually acquire and release the device to avoid changing any ref counts.
        auto apiDevice = Device::Acquire(*device);
        (*cb)(apiDevice, static_cast<ErrorType>(type), message, static_cast<T>(userdata_param));
        apiDevice.MoveToCHandle();
    };
    uncapturedErrorCallbackInfo.userdata1 = reinterpret_cast<void*>(+callback);
    uncapturedErrorCallbackInfo.userdata2 = reinterpret_cast<void*>(userdata);
}

template <typename L, typename Cb, typename>
void DeviceDescriptor::SetUncapturedErrorCallback(L callback) {
    assert(uncapturedErrorCallbackInfo.callback == nullptr);
    using F = UncapturedErrorCallback<void>;
    static_assert(std::is_convertible_v<L, F*>, "Uncaptured error callback cannot be a binding lambda");

    uncapturedErrorCallbackInfo.callback = [](WGPUDevice const * device, WGPUErrorType type, WGPUStringView message, void* callback_param, void*) {
        auto cb = reinterpret_cast<F*>(callback_param);
        // We manually acquire and release the device to avoid changing any ref counts.
        auto apiDevice = Device::Acquire(*device);
        (*cb)(apiDevice, static_cast<ErrorType>(type), message);
        apiDevice.MoveToCHandle();
    };
    uncapturedErrorCallbackInfo.userdata1 = reinterpret_cast<void*>(+callback);
    uncapturedErrorCallbackInfo.userdata2 = nullptr;
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

// Adapter implementation

void Adapter::GetFeatures(SupportedFeatures * features) const {
    *features = SupportedFeatures();
    wgpuAdapterGetFeatures(Get(), reinterpret_cast<WGPUSupportedFeatures *>(features));
}
ConvertibleStatus Adapter::GetInfo(AdapterInfo * info) const {
    *info = AdapterInfo();
    auto result = wgpuAdapterGetInfo(Get(), reinterpret_cast<WGPUAdapterInfo *>(info));
    return static_cast<Status>(result);
}
ConvertibleStatus Adapter::GetLimits(Limits * limits) const {
    auto result = wgpuAdapterGetLimits(Get(), reinterpret_cast<WGPULimits *>(limits));
    return static_cast<Status>(result);
}
Bool Adapter::HasFeature(FeatureName feature) const {
    auto result = wgpuAdapterHasFeature(Get(), static_cast<WGPUFeatureName>(feature));
    return result;
}
template <typename F, typename T,
          typename Cb,
          typename CbChar,
          typename>
Future Adapter::RequestDevice(DeviceDescriptor const * descriptor, CallbackMode callbackMode,F callback, T userdata) const {
    WGPURequestDeviceCallbackInfo callbackInfo = {};
    callbackInfo.mode = static_cast<WGPUCallbackMode>(callbackMode);
    if constexpr (std::is_convertible_v<F, Cb*>) {
        callbackInfo.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* callback_param, void* userdata_param) {
            auto cb = reinterpret_cast<Cb*>(callback_param);
            (*cb)(static_cast<RequestDeviceStatus>(status), Device::Acquire(device), StringView {
    message.data,
    message.length
}, static_cast<T>(userdata_param));
        };
    } else {
        callbackInfo.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* callback_param, void* userdata_param) {
            auto cb = reinterpret_cast<CbChar*>(callback_param);
            (*cb)(static_cast<RequestDeviceStatus>(status), Device::Acquire(device), {detail::StringViewAdapter(message)}, static_cast<T>(userdata_param));
        };
    }
    callbackInfo.userdata1 = reinterpret_cast<void*>(+callback);
    callbackInfo.userdata2 = reinterpret_cast<void*>(userdata);
    auto result = wgpuAdapterRequestDevice(Get(), reinterpret_cast<WGPUDeviceDescriptor const *>(descriptor), callbackInfo);
    return Future {
        result.id
    };
}
template <typename L,
          typename Cb,
          typename CbChar,
          typename>
Future Adapter::RequestDevice(DeviceDescriptor const * descriptor, CallbackMode callbackMode,L callback) const {
    using F = RequestDeviceCallback<void>;

    WGPURequestDeviceCallbackInfo callbackInfo = {};
    callbackInfo.mode = static_cast<WGPUCallbackMode>(callbackMode);
    if constexpr (std::is_convertible_v<L, F*>) {
        callbackInfo.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* callback_param, void*) {
            auto cb = reinterpret_cast<F*>(callback_param);
            (*cb)(static_cast<RequestDeviceStatus>(status), Device::Acquire(device), StringView {
    message.data,
    message.length
});
        };
        callbackInfo.userdata1 = reinterpret_cast<void*>(+callback);
        callbackInfo.userdata2 = nullptr;
    } else {
        auto* lambda = new L(std::move(callback));
        callbackInfo.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* callback_param, void*) {
            std::unique_ptr<L> the_lambda(reinterpret_cast<L*>(callback_param));
            (*the_lambda)(static_cast<RequestDeviceStatus>(status), Device::Acquire(device), {detail::StringViewAdapter(message)});
        };
        callbackInfo.userdata1 = reinterpret_cast<void*>(lambda);
        callbackInfo.userdata2 = nullptr;
    }
    auto result = wgpuAdapterRequestDevice(Get(), reinterpret_cast<WGPUDeviceDescriptor const *>(descriptor), callbackInfo);
    return Future {
            result.id
        };
}


void Adapter::WGPUAddRef(WGPUAdapter handle) {
    if (handle != nullptr) {
        wgpuAdapterAddRef(handle);
    }
}
void Adapter::WGPURelease(WGPUAdapter handle) {
    if (handle != nullptr) {
        wgpuAdapterRelease(handle);
    }
}
static_assert(sizeof(Adapter) == sizeof(WGPUAdapter), "sizeof mismatch for Adapter");
static_assert(alignof(Adapter) == alignof(WGPUAdapter), "alignof mismatch for Adapter");

// BindGroup implementation

void BindGroup::SetLabel(StringView label) const {
    wgpuBindGroupSetLabel(Get(), *reinterpret_cast<WGPUStringView const*>(&label));
}


void BindGroup::WGPUAddRef(WGPUBindGroup handle) {
    if (handle != nullptr) {
        wgpuBindGroupAddRef(handle);
    }
}
void BindGroup::WGPURelease(WGPUBindGroup handle) {
    if (handle != nullptr) {
        wgpuBindGroupRelease(handle);
    }
}
static_assert(sizeof(BindGroup) == sizeof(WGPUBindGroup), "sizeof mismatch for BindGroup");
static_assert(alignof(BindGroup) == alignof(WGPUBindGroup), "alignof mismatch for BindGroup");

// BindGroupLayout implementation

void BindGroupLayout::SetLabel(StringView label) const {
    wgpuBindGroupLayoutSetLabel(Get(), *reinterpret_cast<WGPUStringView const*>(&label));
}


void BindGroupLayout::WGPUAddRef(WGPUBindGroupLayout handle) {
    if (handle != nullptr) {
        wgpuBindGroupLayoutAddRef(handle);
    }
}
void BindGroupLayout::WGPURelease(WGPUBindGroupLayout handle) {
    if (handle != nullptr) {
        wgpuBindGroupLayoutRelease(handle);
    }
}
static_assert(sizeof(BindGroupLayout) == sizeof(WGPUBindGroupLayout), "sizeof mismatch for BindGroupLayout");
static_assert(alignof(BindGroupLayout) == alignof(WGPUBindGroupLayout), "alignof mismatch for BindGroupLayout");

// Buffer implementation

void Buffer::Destroy() const {
    wgpuBufferDestroy(Get());
}
void const * Buffer::GetConstMappedRange(size_t offset, size_t size) const {
    auto result = wgpuBufferGetConstMappedRange(Get(), offset, size);
    return result;
}
void * Buffer::GetMappedRange(size_t offset, size_t size) const {
    auto result = wgpuBufferGetMappedRange(Get(), offset, size);
    return result;
}
BufferMapState Buffer::GetMapState() const {
    auto result = wgpuBufferGetMapState(Get());
    return static_cast<BufferMapState>(result);
}
uint64_t Buffer::GetSize() const {
    auto result = wgpuBufferGetSize(Get());
    return result;
}
BufferUsage Buffer::GetUsage() const {
    auto result = wgpuBufferGetUsage(Get());
    return static_cast<BufferUsage>(result);
}
template <typename F, typename T,
          typename Cb,
          typename CbChar,
          typename>
Future Buffer::MapAsync(MapMode mode, size_t offset, size_t size, CallbackMode callbackMode,F callback, T userdata) const {
    WGPUBufferMapCallbackInfo callbackInfo = {};
    callbackInfo.mode = static_cast<WGPUCallbackMode>(callbackMode);
    if constexpr (std::is_convertible_v<F, Cb*>) {
        callbackInfo.callback = [](WGPUMapAsyncStatus status, WGPUStringView message, void* callback_param, void* userdata_param) {
            auto cb = reinterpret_cast<Cb*>(callback_param);
            (*cb)(static_cast<MapAsyncStatus>(status), StringView {
    message.data,
    message.length
}, static_cast<T>(userdata_param));
        };
    } else {
        callbackInfo.callback = [](WGPUMapAsyncStatus status, WGPUStringView message, void* callback_param, void* userdata_param) {
            auto cb = reinterpret_cast<CbChar*>(callback_param);
            (*cb)(static_cast<MapAsyncStatus>(status), {detail::StringViewAdapter(message)}, static_cast<T>(userdata_param));
        };
    }
    callbackInfo.userdata1 = reinterpret_cast<void*>(+callback);
    callbackInfo.userdata2 = reinterpret_cast<void*>(userdata);
    auto result = wgpuBufferMapAsync(Get(), static_cast<WGPUMapMode>(mode), offset, size, callbackInfo);
    return Future {
        result.id
    };
}
template <typename L,
          typename Cb,
          typename CbChar,
          typename>
Future Buffer::MapAsync(MapMode mode, size_t offset, size_t size, CallbackMode callbackMode,L callback) const {
    using F = BufferMapCallback<void>;

    WGPUBufferMapCallbackInfo callbackInfo = {};
    callbackInfo.mode = static_cast<WGPUCallbackMode>(callbackMode);
    if constexpr (std::is_convertible_v<L, F*>) {
        callbackInfo.callback = [](WGPUMapAsyncStatus status, WGPUStringView message, void* callback_param, void*) {
            auto cb = reinterpret_cast<F*>(callback_param);
            (*cb)(static_cast<MapAsyncStatus>(status), StringView {
    message.data,
    message.length
});
        };
        callbackInfo.userdata1 = reinterpret_cast<void*>(+callback);
        callbackInfo.userdata2 = nullptr;
    } else {
        auto* lambda = new L(std::move(callback));
        callbackInfo.callback = [](WGPUMapAsyncStatus status, WGPUStringView message, void* callback_param, void*) {
            std::unique_ptr<L> the_lambda(reinterpret_cast<L*>(callback_param));
            (*the_lambda)(static_cast<MapAsyncStatus>(status), {detail::StringViewAdapter(message)});
        };
        callbackInfo.userdata1 = reinterpret_cast<void*>(lambda);
        callbackInfo.userdata2 = nullptr;
    }
    auto result = wgpuBufferMapAsync(Get(), static_cast<WGPUMapMode>(mode), offset, size, callbackInfo);
    return Future {
            result.id
        };
}
ConvertibleStatus Buffer::ReadMappedRange(size_t offset, void * data, size_t size) const {
    auto result = wgpuBufferReadMappedRange(Get(), offset, reinterpret_cast<void *>(data), size);
    return static_cast<Status>(result);
}
void Buffer::SetLabel(StringView label) const {
    wgpuBufferSetLabel(Get(), *reinterpret_cast<WGPUStringView const*>(&label));
}
void Buffer::Unmap() const {
    wgpuBufferUnmap(Get());
}
ConvertibleStatus Buffer::WriteMappedRange(size_t offset, void const * data, size_t size) const {
    auto result = wgpuBufferWriteMappedRange(Get(), offset, reinterpret_cast<void const *>(data), size);
    return static_cast<Status>(result);
}


void Buffer::WGPUAddRef(WGPUBuffer handle) {
    if (handle != nullptr) {
        wgpuBufferAddRef(handle);
    }
}
void Buffer::WGPURelease(WGPUBuffer handle) {
    if (handle != nullptr) {
        wgpuBufferRelease(handle);
    }
}
static_assert(sizeof(Buffer) == sizeof(WGPUBuffer), "sizeof mismatch for Buffer");
static_assert(alignof(Buffer) == alignof(WGPUBuffer), "alignof mismatch for Buffer");

// CommandBuffer implementation

void CommandBuffer::SetLabel(StringView label) const {
    wgpuCommandBufferSetLabel(Get(), *reinterpret_cast<WGPUStringView const*>(&label));
}


void CommandBuffer::WGPUAddRef(WGPUCommandBuffer handle) {
    if (handle != nullptr) {
        wgpuCommandBufferAddRef(handle);
    }
}
void CommandBuffer::WGPURelease(WGPUCommandBuffer handle) {
    if (handle != nullptr) {
        wgpuCommandBufferRelease(handle);
    }
}
static_assert(sizeof(CommandBuffer) == sizeof(WGPUCommandBuffer), "sizeof mismatch for CommandBuffer");
static_assert(alignof(CommandBuffer) == alignof(WGPUCommandBuffer), "alignof mismatch for CommandBuffer");

// CommandEncoder implementation

ComputePassEncoder CommandEncoder::BeginComputePass(ComputePassDescriptor const * descriptor) const {
    auto result = wgpuCommandEncoderBeginComputePass(Get(), reinterpret_cast<WGPUComputePassDescriptor const *>(descriptor));
    return ComputePassEncoder::Acquire(result);
}
RenderPassEncoder CommandEncoder::BeginRenderPass(RenderPassDescriptor const * descriptor) const {
    auto result = wgpuCommandEncoderBeginRenderPass(Get(), reinterpret_cast<WGPURenderPassDescriptor const *>(descriptor));
    return RenderPassEncoder::Acquire(result);
}
void CommandEncoder::ClearBuffer(Buffer const& buffer, uint64_t offset, uint64_t size) const {
    wgpuCommandEncoderClearBuffer(Get(), buffer.Get(), offset, size);
}
void CommandEncoder::CopyBufferToBuffer(Buffer const& source, uint64_t sourceOffset, Buffer const& destination, uint64_t destinationOffset, uint64_t size) const {
    wgpuCommandEncoderCopyBufferToBuffer(Get(), source.Get(), sourceOffset, destination.Get(), destinationOffset, size);
}
void CommandEncoder::CopyBufferToTexture(TexelCopyBufferInfo const * source, TexelCopyTextureInfo const * destination, Extent3D const * copySize) const {
    wgpuCommandEncoderCopyBufferToTexture(Get(), reinterpret_cast<WGPUTexelCopyBufferInfo const *>(source), reinterpret_cast<WGPUTexelCopyTextureInfo const *>(destination), reinterpret_cast<WGPUExtent3D const *>(copySize));
}
void CommandEncoder::CopyTextureToBuffer(TexelCopyTextureInfo const * source, TexelCopyBufferInfo const * destination, Extent3D const * copySize) const {
    wgpuCommandEncoderCopyTextureToBuffer(Get(), reinterpret_cast<WGPUTexelCopyTextureInfo const *>(source), reinterpret_cast<WGPUTexelCopyBufferInfo const *>(destination), reinterpret_cast<WGPUExtent3D const *>(copySize));
}
void CommandEncoder::CopyTextureToTexture(TexelCopyTextureInfo const * source, TexelCopyTextureInfo const * destination, Extent3D const * copySize) const {
    wgpuCommandEncoderCopyTextureToTexture(Get(), reinterpret_cast<WGPUTexelCopyTextureInfo const *>(source), reinterpret_cast<WGPUTexelCopyTextureInfo const *>(destination), reinterpret_cast<WGPUExtent3D const *>(copySize));
}
CommandBuffer CommandEncoder::Finish(CommandBufferDescriptor const * descriptor) const {
    auto result = wgpuCommandEncoderFinish(Get(), reinterpret_cast<WGPUCommandBufferDescriptor const *>(descriptor));
    return CommandBuffer::Acquire(result);
}
void CommandEncoder::InsertDebugMarker(StringView markerLabel) const {
    wgpuCommandEncoderInsertDebugMarker(Get(), *reinterpret_cast<WGPUStringView const*>(&markerLabel));
}
void CommandEncoder::PopDebugGroup() const {
    wgpuCommandEncoderPopDebugGroup(Get());
}
void CommandEncoder::PushDebugGroup(StringView groupLabel) const {
    wgpuCommandEncoderPushDebugGroup(Get(), *reinterpret_cast<WGPUStringView const*>(&groupLabel));
}
void CommandEncoder::ResolveQuerySet(QuerySet const& querySet, uint32_t firstQuery, uint32_t queryCount, Buffer const& destination, uint64_t destinationOffset) const {
    wgpuCommandEncoderResolveQuerySet(Get(), querySet.Get(), firstQuery, queryCount, destination.Get(), destinationOffset);
}
void CommandEncoder::SetLabel(StringView label) const {
    wgpuCommandEncoderSetLabel(Get(), *reinterpret_cast<WGPUStringView const*>(&label));
}
void CommandEncoder::WriteTimestamp(QuerySet const& querySet, uint32_t queryIndex) const {
    wgpuCommandEncoderWriteTimestamp(Get(), querySet.Get(), queryIndex);
}


void CommandEncoder::WGPUAddRef(WGPUCommandEncoder handle) {
    if (handle != nullptr) {
        wgpuCommandEncoderAddRef(handle);
    }
}
void CommandEncoder::WGPURelease(WGPUCommandEncoder handle) {
    if (handle != nullptr) {
        wgpuCommandEncoderRelease(handle);
    }
}
static_assert(sizeof(CommandEncoder) == sizeof(WGPUCommandEncoder), "sizeof mismatch for CommandEncoder");
static_assert(alignof(CommandEncoder) == alignof(WGPUCommandEncoder), "alignof mismatch for CommandEncoder");

// ComputePassEncoder implementation

void ComputePassEncoder::DispatchWorkgroups(uint32_t workgroupCountX, uint32_t workgroupCountY, uint32_t workgroupCountZ) const {
    wgpuComputePassEncoderDispatchWorkgroups(Get(), workgroupCountX, workgroupCountY, workgroupCountZ);
}
void ComputePassEncoder::DispatchWorkgroupsIndirect(Buffer const& indirectBuffer, uint64_t indirectOffset) const {
    wgpuComputePassEncoderDispatchWorkgroupsIndirect(Get(), indirectBuffer.Get(), indirectOffset);
}
void ComputePassEncoder::End() const {
    wgpuComputePassEncoderEnd(Get());
}
void ComputePassEncoder::InsertDebugMarker(StringView markerLabel) const {
    wgpuComputePassEncoderInsertDebugMarker(Get(), *reinterpret_cast<WGPUStringView const*>(&markerLabel));
}
void ComputePassEncoder::PopDebugGroup() const {
    wgpuComputePassEncoderPopDebugGroup(Get());
}
void ComputePassEncoder::PushDebugGroup(StringView groupLabel) const {
    wgpuComputePassEncoderPushDebugGroup(Get(), *reinterpret_cast<WGPUStringView const*>(&groupLabel));
}
void ComputePassEncoder::SetBindGroup(uint32_t groupIndex, BindGroup const& group, size_t dynamicOffsetCount, uint32_t const * dynamicOffsets) const {
    wgpuComputePassEncoderSetBindGroup(Get(), groupIndex, group.Get(), dynamicOffsetCount, reinterpret_cast<uint32_t const *>(dynamicOffsets));
}
void ComputePassEncoder::SetLabel(StringView label) const {
    wgpuComputePassEncoderSetLabel(Get(), *reinterpret_cast<WGPUStringView const*>(&label));
}
void ComputePassEncoder::SetPipeline(ComputePipeline const& pipeline) const {
    wgpuComputePassEncoderSetPipeline(Get(), pipeline.Get());
}
void ComputePassEncoder::WriteTimestamp(QuerySet const& querySet, uint32_t queryIndex) const {
    wgpuComputePassEncoderWriteTimestamp(Get(), querySet.Get(), queryIndex);
}


void ComputePassEncoder::WGPUAddRef(WGPUComputePassEncoder handle) {
    if (handle != nullptr) {
        wgpuComputePassEncoderAddRef(handle);
    }
}
void ComputePassEncoder::WGPURelease(WGPUComputePassEncoder handle) {
    if (handle != nullptr) {
        wgpuComputePassEncoderRelease(handle);
    }
}
static_assert(sizeof(ComputePassEncoder) == sizeof(WGPUComputePassEncoder), "sizeof mismatch for ComputePassEncoder");
static_assert(alignof(ComputePassEncoder) == alignof(WGPUComputePassEncoder), "alignof mismatch for ComputePassEncoder");

// ComputePipeline implementation

BindGroupLayout ComputePipeline::GetBindGroupLayout(uint32_t groupIndex) const {
    auto result = wgpuComputePipelineGetBindGroupLayout(Get(), groupIndex);
    return BindGroupLayout::Acquire(result);
}
void ComputePipeline::SetLabel(StringView label) const {
    wgpuComputePipelineSetLabel(Get(), *reinterpret_cast<WGPUStringView const*>(&label));
}


void ComputePipeline::WGPUAddRef(WGPUComputePipeline handle) {
    if (handle != nullptr) {
        wgpuComputePipelineAddRef(handle);
    }
}
void ComputePipeline::WGPURelease(WGPUComputePipeline handle) {
    if (handle != nullptr) {
        wgpuComputePipelineRelease(handle);
    }
}
static_assert(sizeof(ComputePipeline) == sizeof(WGPUComputePipeline), "sizeof mismatch for ComputePipeline");
static_assert(alignof(ComputePipeline) == alignof(WGPUComputePipeline), "alignof mismatch for ComputePipeline");

// Device implementation

BindGroup Device::CreateBindGroup(BindGroupDescriptor const * descriptor) const {
    auto result = wgpuDeviceCreateBindGroup(Get(), reinterpret_cast<WGPUBindGroupDescriptor const *>(descriptor));
    return BindGroup::Acquire(result);
}
BindGroupLayout Device::CreateBindGroupLayout(BindGroupLayoutDescriptor const * descriptor) const {
    auto result = wgpuDeviceCreateBindGroupLayout(Get(), reinterpret_cast<WGPUBindGroupLayoutDescriptor const *>(descriptor));
    return BindGroupLayout::Acquire(result);
}
Buffer Device::CreateBuffer(BufferDescriptor const * descriptor) const {
    auto result = wgpuDeviceCreateBuffer(Get(), reinterpret_cast<WGPUBufferDescriptor const *>(descriptor));
    return Buffer::Acquire(result);
}
CommandEncoder Device::CreateCommandEncoder(CommandEncoderDescriptor const * descriptor) const {
    auto result = wgpuDeviceCreateCommandEncoder(Get(), reinterpret_cast<WGPUCommandEncoderDescriptor const *>(descriptor));
    return CommandEncoder::Acquire(result);
}
ComputePipeline Device::CreateComputePipeline(ComputePipelineDescriptor const * descriptor) const {
    auto result = wgpuDeviceCreateComputePipeline(Get(), reinterpret_cast<WGPUComputePipelineDescriptor const *>(descriptor));
    return ComputePipeline::Acquire(result);
}
template <typename F, typename T,
          typename Cb,
          typename CbChar,
          typename>
Future Device::CreateComputePipelineAsync(ComputePipelineDescriptor const * descriptor, CallbackMode callbackMode,F callback, T userdata) const {
    WGPUCreateComputePipelineAsyncCallbackInfo callbackInfo = {};
    callbackInfo.mode = static_cast<WGPUCallbackMode>(callbackMode);
    if constexpr (std::is_convertible_v<F, Cb*>) {
        callbackInfo.callback = [](WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline pipeline, WGPUStringView message, void* callback_param, void* userdata_param) {
            auto cb = reinterpret_cast<Cb*>(callback_param);
            (*cb)(static_cast<CreatePipelineAsyncStatus>(status), ComputePipeline::Acquire(pipeline), StringView {
    message.data,
    message.length
}, static_cast<T>(userdata_param));
        };
    } else {
        callbackInfo.callback = [](WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline pipeline, WGPUStringView message, void* callback_param, void* userdata_param) {
            auto cb = reinterpret_cast<CbChar*>(callback_param);
            (*cb)(static_cast<CreatePipelineAsyncStatus>(status), ComputePipeline::Acquire(pipeline), {detail::StringViewAdapter(message)}, static_cast<T>(userdata_param));
        };
    }
    callbackInfo.userdata1 = reinterpret_cast<void*>(+callback);
    callbackInfo.userdata2 = reinterpret_cast<void*>(userdata);
    auto result = wgpuDeviceCreateComputePipelineAsync(Get(), reinterpret_cast<WGPUComputePipelineDescriptor const *>(descriptor), callbackInfo);
    return Future {
        result.id
    };
}
template <typename L,
          typename Cb,
          typename CbChar,
          typename>
Future Device::CreateComputePipelineAsync(ComputePipelineDescriptor const * descriptor, CallbackMode callbackMode,L callback) const {
    using F = CreateComputePipelineAsyncCallback<void>;

    WGPUCreateComputePipelineAsyncCallbackInfo callbackInfo = {};
    callbackInfo.mode = static_cast<WGPUCallbackMode>(callbackMode);
    if constexpr (std::is_convertible_v<L, F*>) {
        callbackInfo.callback = [](WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline pipeline, WGPUStringView message, void* callback_param, void*) {
            auto cb = reinterpret_cast<F*>(callback_param);
            (*cb)(static_cast<CreatePipelineAsyncStatus>(status), ComputePipeline::Acquire(pipeline), StringView {
    message.data,
    message.length
});
        };
        callbackInfo.userdata1 = reinterpret_cast<void*>(+callback);
        callbackInfo.userdata2 = nullptr;
    } else {
        auto* lambda = new L(std::move(callback));
        callbackInfo.callback = [](WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline pipeline, WGPUStringView message, void* callback_param, void*) {
            std::unique_ptr<L> the_lambda(reinterpret_cast<L*>(callback_param));
            (*the_lambda)(static_cast<CreatePipelineAsyncStatus>(status), ComputePipeline::Acquire(pipeline), {detail::StringViewAdapter(message)});
        };
        callbackInfo.userdata1 = reinterpret_cast<void*>(lambda);
        callbackInfo.userdata2 = nullptr;
    }
    auto result = wgpuDeviceCreateComputePipelineAsync(Get(), reinterpret_cast<WGPUComputePipelineDescriptor const *>(descriptor), callbackInfo);
    return Future {
            result.id
        };
}
PipelineLayout Device::CreatePipelineLayout(PipelineLayoutDescriptor const * descriptor) const {
    auto result = wgpuDeviceCreatePipelineLayout(Get(), reinterpret_cast<WGPUPipelineLayoutDescriptor const *>(descriptor));
    return PipelineLayout::Acquire(result);
}
QuerySet Device::CreateQuerySet(QuerySetDescriptor const * descriptor) const {
    auto result = wgpuDeviceCreateQuerySet(Get(), reinterpret_cast<WGPUQuerySetDescriptor const *>(descriptor));
    return QuerySet::Acquire(result);
}
RenderBundleEncoder Device::CreateRenderBundleEncoder(RenderBundleEncoderDescriptor const * descriptor) const {
    auto result = wgpuDeviceCreateRenderBundleEncoder(Get(), reinterpret_cast<WGPURenderBundleEncoderDescriptor const *>(descriptor));
    return RenderBundleEncoder::Acquire(result);
}
RenderPipeline Device::CreateRenderPipeline(RenderPipelineDescriptor const * descriptor) const {
    auto result = wgpuDeviceCreateRenderPipeline(Get(), reinterpret_cast<WGPURenderPipelineDescriptor const *>(descriptor));
    return RenderPipeline::Acquire(result);
}
template <typename F, typename T,
          typename Cb,
          typename CbChar,
          typename>
Future Device::CreateRenderPipelineAsync(RenderPipelineDescriptor const * descriptor, CallbackMode callbackMode,F callback, T userdata) const {
    WGPUCreateRenderPipelineAsyncCallbackInfo callbackInfo = {};
    callbackInfo.mode = static_cast<WGPUCallbackMode>(callbackMode);
    if constexpr (std::is_convertible_v<F, Cb*>) {
        callbackInfo.callback = [](WGPUCreatePipelineAsyncStatus status, WGPURenderPipeline pipeline, WGPUStringView message, void* callback_param, void* userdata_param) {
            auto cb = reinterpret_cast<Cb*>(callback_param);
            (*cb)(static_cast<CreatePipelineAsyncStatus>(status), RenderPipeline::Acquire(pipeline), StringView {
    message.data,
    message.length
}, static_cast<T>(userdata_param));
        };
    } else {
        callbackInfo.callback = [](WGPUCreatePipelineAsyncStatus status, WGPURenderPipeline pipeline, WGPUStringView message, void* callback_param, void* userdata_param) {
            auto cb = reinterpret_cast<CbChar*>(callback_param);
            (*cb)(static_cast<CreatePipelineAsyncStatus>(status), RenderPipeline::Acquire(pipeline), {detail::StringViewAdapter(message)}, static_cast<T>(userdata_param));
        };
    }
    callbackInfo.userdata1 = reinterpret_cast<void*>(+callback);
    callbackInfo.userdata2 = reinterpret_cast<void*>(userdata);
    auto result = wgpuDeviceCreateRenderPipelineAsync(Get(), reinterpret_cast<WGPURenderPipelineDescriptor const *>(descriptor), callbackInfo);
    return Future {
        result.id
    };
}
template <typename L,
          typename Cb,
          typename CbChar,
          typename>
Future Device::CreateRenderPipelineAsync(RenderPipelineDescriptor const * descriptor, CallbackMode callbackMode,L callback) const {
    using F = CreateRenderPipelineAsyncCallback<void>;

    WGPUCreateRenderPipelineAsyncCallbackInfo callbackInfo = {};
    callbackInfo.mode = static_cast<WGPUCallbackMode>(callbackMode);
    if constexpr (std::is_convertible_v<L, F*>) {
        callbackInfo.callback = [](WGPUCreatePipelineAsyncStatus status, WGPURenderPipeline pipeline, WGPUStringView message, void* callback_param, void*) {
            auto cb = reinterpret_cast<F*>(callback_param);
            (*cb)(static_cast<CreatePipelineAsyncStatus>(status), RenderPipeline::Acquire(pipeline), StringView {
    message.data,
    message.length
});
        };
        callbackInfo.userdata1 = reinterpret_cast<void*>(+callback);
        callbackInfo.userdata2 = nullptr;
    } else {
        auto* lambda = new L(std::move(callback));
        callbackInfo.callback = [](WGPUCreatePipelineAsyncStatus status, WGPURenderPipeline pipeline, WGPUStringView message, void* callback_param, void*) {
            std::unique_ptr<L> the_lambda(reinterpret_cast<L*>(callback_param));
            (*the_lambda)(static_cast<CreatePipelineAsyncStatus>(status), RenderPipeline::Acquire(pipeline), {detail::StringViewAdapter(message)});
        };
        callbackInfo.userdata1 = reinterpret_cast<void*>(lambda);
        callbackInfo.userdata2 = nullptr;
    }
    auto result = wgpuDeviceCreateRenderPipelineAsync(Get(), reinterpret_cast<WGPURenderPipelineDescriptor const *>(descriptor), callbackInfo);
    return Future {
            result.id
        };
}
Sampler Device::CreateSampler(SamplerDescriptor const * descriptor) const {
    auto result = wgpuDeviceCreateSampler(Get(), reinterpret_cast<WGPUSamplerDescriptor const *>(descriptor));
    return Sampler::Acquire(result);
}
ShaderModule Device::CreateShaderModule(ShaderModuleDescriptor const * descriptor) const {
    auto result = wgpuDeviceCreateShaderModule(Get(), reinterpret_cast<WGPUShaderModuleDescriptor const *>(descriptor));
    return ShaderModule::Acquire(result);
}
Texture Device::CreateTexture(TextureDescriptor const * descriptor) const {
    auto result = wgpuDeviceCreateTexture(Get(), reinterpret_cast<WGPUTextureDescriptor const *>(descriptor));
    return Texture::Acquire(result);
}
void Device::Destroy() const {
    wgpuDeviceDestroy(Get());
}
ConvertibleStatus Device::GetAdapterInfo(AdapterInfo * adapterInfo) const {
    *adapterInfo = AdapterInfo();
    auto result = wgpuDeviceGetAdapterInfo(Get(), reinterpret_cast<WGPUAdapterInfo *>(adapterInfo));
    return static_cast<Status>(result);
}
void Device::GetFeatures(SupportedFeatures * features) const {
    *features = SupportedFeatures();
    wgpuDeviceGetFeatures(Get(), reinterpret_cast<WGPUSupportedFeatures *>(features));
}
ConvertibleStatus Device::GetLimits(Limits * limits) const {
    auto result = wgpuDeviceGetLimits(Get(), reinterpret_cast<WGPULimits *>(limits));
    return static_cast<Status>(result);
}
Future Device::GetLostFuture() const {
    auto result = wgpuDeviceGetLostFuture(Get());
    return Future {
            result.id
        };
}
Queue Device::GetQueue() const {
    auto result = wgpuDeviceGetQueue(Get());
    return Queue::Acquire(result);
}
Bool Device::HasFeature(FeatureName feature) const {
    auto result = wgpuDeviceHasFeature(Get(), static_cast<WGPUFeatureName>(feature));
    return result;
}
template <typename F, typename T,
          typename Cb,
          typename CbChar,
          typename>
Future Device::PopErrorScope(CallbackMode callbackMode,F callback, T userdata) const {
    WGPUPopErrorScopeCallbackInfo callbackInfo = {};
    callbackInfo.mode = static_cast<WGPUCallbackMode>(callbackMode);
    if constexpr (std::is_convertible_v<F, Cb*>) {
        callbackInfo.callback = [](WGPUPopErrorScopeStatus status, WGPUErrorType type, WGPUStringView message, void* callback_param, void* userdata_param) {
            auto cb = reinterpret_cast<Cb*>(callback_param);
            (*cb)(static_cast<PopErrorScopeStatus>(status), static_cast<ErrorType>(type), StringView {
    message.data,
    message.length
}, static_cast<T>(userdata_param));
        };
    } else {
        callbackInfo.callback = [](WGPUPopErrorScopeStatus status, WGPUErrorType type, WGPUStringView message, void* callback_param, void* userdata_param) {
            auto cb = reinterpret_cast<CbChar*>(callback_param);
            (*cb)(static_cast<PopErrorScopeStatus>(status), static_cast<ErrorType>(type), {detail::StringViewAdapter(message)}, static_cast<T>(userdata_param));
        };
    }
    callbackInfo.userdata1 = reinterpret_cast<void*>(+callback);
    callbackInfo.userdata2 = reinterpret_cast<void*>(userdata);
    auto result = wgpuDevicePopErrorScope(Get(), callbackInfo);
    return Future {
        result.id
    };
}
template <typename L,
          typename Cb,
          typename CbChar,
          typename>
Future Device::PopErrorScope(CallbackMode callbackMode,L callback) const {
    using F = PopErrorScopeCallback<void>;

    WGPUPopErrorScopeCallbackInfo callbackInfo = {};
    callbackInfo.mode = static_cast<WGPUCallbackMode>(callbackMode);
    if constexpr (std::is_convertible_v<L, F*>) {
        callbackInfo.callback = [](WGPUPopErrorScopeStatus status, WGPUErrorType type, WGPUStringView message, void* callback_param, void*) {
            auto cb = reinterpret_cast<F*>(callback_param);
            (*cb)(static_cast<PopErrorScopeStatus>(status), static_cast<ErrorType>(type), StringView {
    message.data,
    message.length
});
        };
        callbackInfo.userdata1 = reinterpret_cast<void*>(+callback);
        callbackInfo.userdata2 = nullptr;
    } else {
        auto* lambda = new L(std::move(callback));
        callbackInfo.callback = [](WGPUPopErrorScopeStatus status, WGPUErrorType type, WGPUStringView message, void* callback_param, void*) {
            std::unique_ptr<L> the_lambda(reinterpret_cast<L*>(callback_param));
            (*the_lambda)(static_cast<PopErrorScopeStatus>(status), static_cast<ErrorType>(type), {detail::StringViewAdapter(message)});
        };
        callbackInfo.userdata1 = reinterpret_cast<void*>(lambda);
        callbackInfo.userdata2 = nullptr;
    }
    auto result = wgpuDevicePopErrorScope(Get(), callbackInfo);
    return Future {
            result.id
        };
}
void Device::PushErrorScope(ErrorFilter filter) const {
    wgpuDevicePushErrorScope(Get(), static_cast<WGPUErrorFilter>(filter));
}
void Device::SetLabel(StringView label) const {
    wgpuDeviceSetLabel(Get(), *reinterpret_cast<WGPUStringView const*>(&label));
}


void Device::WGPUAddRef(WGPUDevice handle) {
    if (handle != nullptr) {
        wgpuDeviceAddRef(handle);
    }
}
void Device::WGPURelease(WGPUDevice handle) {
    if (handle != nullptr) {
        wgpuDeviceRelease(handle);
    }
}
static_assert(sizeof(Device) == sizeof(WGPUDevice), "sizeof mismatch for Device");
static_assert(alignof(Device) == alignof(WGPUDevice), "alignof mismatch for Device");

// Instance implementation

Surface Instance::CreateSurface(SurfaceDescriptor const * descriptor) const {
    auto result = wgpuInstanceCreateSurface(Get(), reinterpret_cast<WGPUSurfaceDescriptor const *>(descriptor));
    return Surface::Acquire(result);
}
void Instance::GetWGSLLanguageFeatures(SupportedWGSLLanguageFeatures * features) const {
    *features = SupportedWGSLLanguageFeatures();
    wgpuInstanceGetWGSLLanguageFeatures(Get(), reinterpret_cast<WGPUSupportedWGSLLanguageFeatures *>(features));
}
Bool Instance::HasWGSLLanguageFeature(WGSLLanguageFeatureName feature) const {
    auto result = wgpuInstanceHasWGSLLanguageFeature(Get(), static_cast<WGPUWGSLLanguageFeatureName>(feature));
    return result;
}
void Instance::ProcessEvents() const {
    wgpuInstanceProcessEvents(Get());
}
template <typename F, typename T,
          typename Cb,
          typename CbChar,
          typename>
Future Instance::RequestAdapter(RequestAdapterOptions const * options, CallbackMode callbackMode,F callback, T userdata) const {
    WGPURequestAdapterCallbackInfo callbackInfo = {};
    callbackInfo.mode = static_cast<WGPUCallbackMode>(callbackMode);
    if constexpr (std::is_convertible_v<F, Cb*>) {
        callbackInfo.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* callback_param, void* userdata_param) {
            auto cb = reinterpret_cast<Cb*>(callback_param);
            (*cb)(static_cast<RequestAdapterStatus>(status), Adapter::Acquire(adapter), StringView {
    message.data,
    message.length
}, static_cast<T>(userdata_param));
        };
    } else {
        callbackInfo.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* callback_param, void* userdata_param) {
            auto cb = reinterpret_cast<CbChar*>(callback_param);
            (*cb)(static_cast<RequestAdapterStatus>(status), Adapter::Acquire(adapter), {detail::StringViewAdapter(message)}, static_cast<T>(userdata_param));
        };
    }
    callbackInfo.userdata1 = reinterpret_cast<void*>(+callback);
    callbackInfo.userdata2 = reinterpret_cast<void*>(userdata);
    auto result = wgpuInstanceRequestAdapter(Get(), reinterpret_cast<WGPURequestAdapterOptions const *>(options), callbackInfo);
    return Future {
        result.id
    };
}
template <typename L,
          typename Cb,
          typename CbChar,
          typename>
Future Instance::RequestAdapter(RequestAdapterOptions const * options, CallbackMode callbackMode,L callback) const {
    using F = RequestAdapterCallback<void>;

    WGPURequestAdapterCallbackInfo callbackInfo = {};
    callbackInfo.mode = static_cast<WGPUCallbackMode>(callbackMode);
    if constexpr (std::is_convertible_v<L, F*>) {
        callbackInfo.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* callback_param, void*) {
            auto cb = reinterpret_cast<F*>(callback_param);
            (*cb)(static_cast<RequestAdapterStatus>(status), Adapter::Acquire(adapter), StringView {
    message.data,
    message.length
});
        };
        callbackInfo.userdata1 = reinterpret_cast<void*>(+callback);
        callbackInfo.userdata2 = nullptr;
    } else {
        auto* lambda = new L(std::move(callback));
        callbackInfo.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* callback_param, void*) {
            std::unique_ptr<L> the_lambda(reinterpret_cast<L*>(callback_param));
            (*the_lambda)(static_cast<RequestAdapterStatus>(status), Adapter::Acquire(adapter), {detail::StringViewAdapter(message)});
        };
        callbackInfo.userdata1 = reinterpret_cast<void*>(lambda);
        callbackInfo.userdata2 = nullptr;
    }
    auto result = wgpuInstanceRequestAdapter(Get(), reinterpret_cast<WGPURequestAdapterOptions const *>(options), callbackInfo);
    return Future {
            result.id
        };
}
WaitStatus Instance::WaitAny(size_t futureCount, FutureWaitInfo * futures, uint64_t timeoutNS) const {
    auto result = wgpuInstanceWaitAny(Get(), futureCount, reinterpret_cast<WGPUFutureWaitInfo *>(futures), timeoutNS);
    return static_cast<WaitStatus>(result);
}

WaitStatus Instance::WaitAny(Future f, uint64_t timeout) const {
    FutureWaitInfo waitInfo { f };
    return WaitAny(1, &waitInfo, timeout);
}

void Instance::WGPUAddRef(WGPUInstance handle) {
    if (handle != nullptr) {
        wgpuInstanceAddRef(handle);
    }
}
void Instance::WGPURelease(WGPUInstance handle) {
    if (handle != nullptr) {
        wgpuInstanceRelease(handle);
    }
}
static_assert(sizeof(Instance) == sizeof(WGPUInstance), "sizeof mismatch for Instance");
static_assert(alignof(Instance) == alignof(WGPUInstance), "alignof mismatch for Instance");

// PipelineLayout implementation

void PipelineLayout::SetLabel(StringView label) const {
    wgpuPipelineLayoutSetLabel(Get(), *reinterpret_cast<WGPUStringView const*>(&label));
}


void PipelineLayout::WGPUAddRef(WGPUPipelineLayout handle) {
    if (handle != nullptr) {
        wgpuPipelineLayoutAddRef(handle);
    }
}
void PipelineLayout::WGPURelease(WGPUPipelineLayout handle) {
    if (handle != nullptr) {
        wgpuPipelineLayoutRelease(handle);
    }
}
static_assert(sizeof(PipelineLayout) == sizeof(WGPUPipelineLayout), "sizeof mismatch for PipelineLayout");
static_assert(alignof(PipelineLayout) == alignof(WGPUPipelineLayout), "alignof mismatch for PipelineLayout");

// QuerySet implementation

void QuerySet::Destroy() const {
    wgpuQuerySetDestroy(Get());
}
uint32_t QuerySet::GetCount() const {
    auto result = wgpuQuerySetGetCount(Get());
    return result;
}
QueryType QuerySet::GetType() const {
    auto result = wgpuQuerySetGetType(Get());
    return static_cast<QueryType>(result);
}
void QuerySet::SetLabel(StringView label) const {
    wgpuQuerySetSetLabel(Get(), *reinterpret_cast<WGPUStringView const*>(&label));
}


void QuerySet::WGPUAddRef(WGPUQuerySet handle) {
    if (handle != nullptr) {
        wgpuQuerySetAddRef(handle);
    }
}
void QuerySet::WGPURelease(WGPUQuerySet handle) {
    if (handle != nullptr) {
        wgpuQuerySetRelease(handle);
    }
}
static_assert(sizeof(QuerySet) == sizeof(WGPUQuerySet), "sizeof mismatch for QuerySet");
static_assert(alignof(QuerySet) == alignof(WGPUQuerySet), "alignof mismatch for QuerySet");

// Queue implementation

template <typename F, typename T,
          typename Cb,
          typename CbChar,
          typename>
Future Queue::OnSubmittedWorkDone(CallbackMode callbackMode,F callback, T userdata) const {
    WGPUQueueWorkDoneCallbackInfo callbackInfo = {};
    callbackInfo.mode = static_cast<WGPUCallbackMode>(callbackMode);
    if constexpr (std::is_convertible_v<F, Cb*>) {
        callbackInfo.callback = [](WGPUQueueWorkDoneStatus status, WGPUStringView message, void* callback_param, void* userdata_param) {
            auto cb = reinterpret_cast<Cb*>(callback_param);
            (*cb)(static_cast<QueueWorkDoneStatus>(status), StringView {
    message.data,
    message.length
}, static_cast<T>(userdata_param));
        };
    } else {
        callbackInfo.callback = [](WGPUQueueWorkDoneStatus status, WGPUStringView message, void* callback_param, void* userdata_param) {
            auto cb = reinterpret_cast<CbChar*>(callback_param);
            (*cb)(static_cast<QueueWorkDoneStatus>(status), {detail::StringViewAdapter(message)}, static_cast<T>(userdata_param));
        };
    }
    callbackInfo.userdata1 = reinterpret_cast<void*>(+callback);
    callbackInfo.userdata2 = reinterpret_cast<void*>(userdata);
    auto result = wgpuQueueOnSubmittedWorkDone(Get(), callbackInfo);
    return Future {
        result.id
    };
}
template <typename L,
          typename Cb,
          typename CbChar,
          typename>
Future Queue::OnSubmittedWorkDone(CallbackMode callbackMode,L callback) const {
    using F = QueueWorkDoneCallback<void>;

    WGPUQueueWorkDoneCallbackInfo callbackInfo = {};
    callbackInfo.mode = static_cast<WGPUCallbackMode>(callbackMode);
    if constexpr (std::is_convertible_v<L, F*>) {
        callbackInfo.callback = [](WGPUQueueWorkDoneStatus status, WGPUStringView message, void* callback_param, void*) {
            auto cb = reinterpret_cast<F*>(callback_param);
            (*cb)(static_cast<QueueWorkDoneStatus>(status), StringView {
    message.data,
    message.length
});
        };
        callbackInfo.userdata1 = reinterpret_cast<void*>(+callback);
        callbackInfo.userdata2 = nullptr;
    } else {
        auto* lambda = new L(std::move(callback));
        callbackInfo.callback = [](WGPUQueueWorkDoneStatus status, WGPUStringView message, void* callback_param, void*) {
            std::unique_ptr<L> the_lambda(reinterpret_cast<L*>(callback_param));
            (*the_lambda)(static_cast<QueueWorkDoneStatus>(status), {detail::StringViewAdapter(message)});
        };
        callbackInfo.userdata1 = reinterpret_cast<void*>(lambda);
        callbackInfo.userdata2 = nullptr;
    }
    auto result = wgpuQueueOnSubmittedWorkDone(Get(), callbackInfo);
    return Future {
            result.id
        };
}
void Queue::SetLabel(StringView label) const {
    wgpuQueueSetLabel(Get(), *reinterpret_cast<WGPUStringView const*>(&label));
}
void Queue::Submit(size_t commandCount, CommandBuffer const * commands) const {
    wgpuQueueSubmit(Get(), commandCount, reinterpret_cast<WGPUCommandBuffer const *>(commands));
}
void Queue::WriteBuffer(Buffer const& buffer, uint64_t bufferOffset, void const * data, size_t size) const {
    wgpuQueueWriteBuffer(Get(), buffer.Get(), bufferOffset, reinterpret_cast<void const *>(data), size);
}
void Queue::WriteTexture(TexelCopyTextureInfo const * destination, void const * data, size_t dataSize, TexelCopyBufferLayout const * dataLayout, Extent3D const * writeSize) const {
    wgpuQueueWriteTexture(Get(), reinterpret_cast<WGPUTexelCopyTextureInfo const *>(destination), reinterpret_cast<void const *>(data), dataSize, reinterpret_cast<WGPUTexelCopyBufferLayout const *>(dataLayout), reinterpret_cast<WGPUExtent3D const *>(writeSize));
}


void Queue::WGPUAddRef(WGPUQueue handle) {
    if (handle != nullptr) {
        wgpuQueueAddRef(handle);
    }
}
void Queue::WGPURelease(WGPUQueue handle) {
    if (handle != nullptr) {
        wgpuQueueRelease(handle);
    }
}
static_assert(sizeof(Queue) == sizeof(WGPUQueue), "sizeof mismatch for Queue");
static_assert(alignof(Queue) == alignof(WGPUQueue), "alignof mismatch for Queue");

// RenderBundle implementation

void RenderBundle::SetLabel(StringView label) const {
    wgpuRenderBundleSetLabel(Get(), *reinterpret_cast<WGPUStringView const*>(&label));
}


void RenderBundle::WGPUAddRef(WGPURenderBundle handle) {
    if (handle != nullptr) {
        wgpuRenderBundleAddRef(handle);
    }
}
void RenderBundle::WGPURelease(WGPURenderBundle handle) {
    if (handle != nullptr) {
        wgpuRenderBundleRelease(handle);
    }
}
static_assert(sizeof(RenderBundle) == sizeof(WGPURenderBundle), "sizeof mismatch for RenderBundle");
static_assert(alignof(RenderBundle) == alignof(WGPURenderBundle), "alignof mismatch for RenderBundle");

// RenderBundleEncoder implementation

void RenderBundleEncoder::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) const {
    wgpuRenderBundleEncoderDraw(Get(), vertexCount, instanceCount, firstVertex, firstInstance);
}
void RenderBundleEncoder::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) const {
    wgpuRenderBundleEncoderDrawIndexed(Get(), indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}
void RenderBundleEncoder::DrawIndexedIndirect(Buffer const& indirectBuffer, uint64_t indirectOffset) const {
    wgpuRenderBundleEncoderDrawIndexedIndirect(Get(), indirectBuffer.Get(), indirectOffset);
}
void RenderBundleEncoder::DrawIndirect(Buffer const& indirectBuffer, uint64_t indirectOffset) const {
    wgpuRenderBundleEncoderDrawIndirect(Get(), indirectBuffer.Get(), indirectOffset);
}
RenderBundle RenderBundleEncoder::Finish(RenderBundleDescriptor const * descriptor) const {
    auto result = wgpuRenderBundleEncoderFinish(Get(), reinterpret_cast<WGPURenderBundleDescriptor const *>(descriptor));
    return RenderBundle::Acquire(result);
}
void RenderBundleEncoder::InsertDebugMarker(StringView markerLabel) const {
    wgpuRenderBundleEncoderInsertDebugMarker(Get(), *reinterpret_cast<WGPUStringView const*>(&markerLabel));
}
void RenderBundleEncoder::PopDebugGroup() const {
    wgpuRenderBundleEncoderPopDebugGroup(Get());
}
void RenderBundleEncoder::PushDebugGroup(StringView groupLabel) const {
    wgpuRenderBundleEncoderPushDebugGroup(Get(), *reinterpret_cast<WGPUStringView const*>(&groupLabel));
}
void RenderBundleEncoder::SetBindGroup(uint32_t groupIndex, BindGroup const& group, size_t dynamicOffsetCount, uint32_t const * dynamicOffsets) const {
    wgpuRenderBundleEncoderSetBindGroup(Get(), groupIndex, group.Get(), dynamicOffsetCount, reinterpret_cast<uint32_t const *>(dynamicOffsets));
}
void RenderBundleEncoder::SetIndexBuffer(Buffer const& buffer, IndexFormat format, uint64_t offset, uint64_t size) const {
    wgpuRenderBundleEncoderSetIndexBuffer(Get(), buffer.Get(), static_cast<WGPUIndexFormat>(format), offset, size);
}
void RenderBundleEncoder::SetLabel(StringView label) const {
    wgpuRenderBundleEncoderSetLabel(Get(), *reinterpret_cast<WGPUStringView const*>(&label));
}
void RenderBundleEncoder::SetPipeline(RenderPipeline const& pipeline) const {
    wgpuRenderBundleEncoderSetPipeline(Get(), pipeline.Get());
}
void RenderBundleEncoder::SetVertexBuffer(uint32_t slot, Buffer const& buffer, uint64_t offset, uint64_t size) const {
    wgpuRenderBundleEncoderSetVertexBuffer(Get(), slot, buffer.Get(), offset, size);
}


void RenderBundleEncoder::WGPUAddRef(WGPURenderBundleEncoder handle) {
    if (handle != nullptr) {
        wgpuRenderBundleEncoderAddRef(handle);
    }
}
void RenderBundleEncoder::WGPURelease(WGPURenderBundleEncoder handle) {
    if (handle != nullptr) {
        wgpuRenderBundleEncoderRelease(handle);
    }
}
static_assert(sizeof(RenderBundleEncoder) == sizeof(WGPURenderBundleEncoder), "sizeof mismatch for RenderBundleEncoder");
static_assert(alignof(RenderBundleEncoder) == alignof(WGPURenderBundleEncoder), "alignof mismatch for RenderBundleEncoder");

// RenderPassEncoder implementation

void RenderPassEncoder::BeginOcclusionQuery(uint32_t queryIndex) const {
    wgpuRenderPassEncoderBeginOcclusionQuery(Get(), queryIndex);
}
void RenderPassEncoder::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) const {
    wgpuRenderPassEncoderDraw(Get(), vertexCount, instanceCount, firstVertex, firstInstance);
}
void RenderPassEncoder::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) const {
    wgpuRenderPassEncoderDrawIndexed(Get(), indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}
void RenderPassEncoder::DrawIndexedIndirect(Buffer const& indirectBuffer, uint64_t indirectOffset) const {
    wgpuRenderPassEncoderDrawIndexedIndirect(Get(), indirectBuffer.Get(), indirectOffset);
}
void RenderPassEncoder::DrawIndirect(Buffer const& indirectBuffer, uint64_t indirectOffset) const {
    wgpuRenderPassEncoderDrawIndirect(Get(), indirectBuffer.Get(), indirectOffset);
}
void RenderPassEncoder::End() const {
    wgpuRenderPassEncoderEnd(Get());
}
void RenderPassEncoder::EndOcclusionQuery() const {
    wgpuRenderPassEncoderEndOcclusionQuery(Get());
}
void RenderPassEncoder::ExecuteBundles(size_t bundleCount, RenderBundle const * bundles) const {
    wgpuRenderPassEncoderExecuteBundles(Get(), bundleCount, reinterpret_cast<WGPURenderBundle const *>(bundles));
}
void RenderPassEncoder::InsertDebugMarker(StringView markerLabel) const {
    wgpuRenderPassEncoderInsertDebugMarker(Get(), *reinterpret_cast<WGPUStringView const*>(&markerLabel));
}
void RenderPassEncoder::MultiDrawIndexedIndirect(Buffer const& indirectBuffer, uint64_t indirectOffset, uint32_t maxDrawCount, Buffer const& drawCountBuffer, uint64_t drawCountBufferOffset) const {
    wgpuRenderPassEncoderMultiDrawIndexedIndirect(Get(), indirectBuffer.Get(), indirectOffset, maxDrawCount, drawCountBuffer.Get(), drawCountBufferOffset);
}
void RenderPassEncoder::MultiDrawIndirect(Buffer const& indirectBuffer, uint64_t indirectOffset, uint32_t maxDrawCount, Buffer const& drawCountBuffer, uint64_t drawCountBufferOffset) const {
    wgpuRenderPassEncoderMultiDrawIndirect(Get(), indirectBuffer.Get(), indirectOffset, maxDrawCount, drawCountBuffer.Get(), drawCountBufferOffset);
}
void RenderPassEncoder::PopDebugGroup() const {
    wgpuRenderPassEncoderPopDebugGroup(Get());
}
void RenderPassEncoder::PushDebugGroup(StringView groupLabel) const {
    wgpuRenderPassEncoderPushDebugGroup(Get(), *reinterpret_cast<WGPUStringView const*>(&groupLabel));
}
void RenderPassEncoder::SetBindGroup(uint32_t groupIndex, BindGroup const& group, size_t dynamicOffsetCount, uint32_t const * dynamicOffsets) const {
    wgpuRenderPassEncoderSetBindGroup(Get(), groupIndex, group.Get(), dynamicOffsetCount, reinterpret_cast<uint32_t const *>(dynamicOffsets));
}
void RenderPassEncoder::SetBlendConstant(Color const * color) const {
    wgpuRenderPassEncoderSetBlendConstant(Get(), reinterpret_cast<WGPUColor const *>(color));
}
void RenderPassEncoder::SetIndexBuffer(Buffer const& buffer, IndexFormat format, uint64_t offset, uint64_t size) const {
    wgpuRenderPassEncoderSetIndexBuffer(Get(), buffer.Get(), static_cast<WGPUIndexFormat>(format), offset, size);
}
void RenderPassEncoder::SetLabel(StringView label) const {
    wgpuRenderPassEncoderSetLabel(Get(), *reinterpret_cast<WGPUStringView const*>(&label));
}
void RenderPassEncoder::SetPipeline(RenderPipeline const& pipeline) const {
    wgpuRenderPassEncoderSetPipeline(Get(), pipeline.Get());
}
void RenderPassEncoder::SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) const {
    wgpuRenderPassEncoderSetScissorRect(Get(), x, y, width, height);
}
void RenderPassEncoder::SetStencilReference(uint32_t reference) const {
    wgpuRenderPassEncoderSetStencilReference(Get(), reference);
}
void RenderPassEncoder::SetVertexBuffer(uint32_t slot, Buffer const& buffer, uint64_t offset, uint64_t size) const {
    wgpuRenderPassEncoderSetVertexBuffer(Get(), slot, buffer.Get(), offset, size);
}
void RenderPassEncoder::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth) const {
    wgpuRenderPassEncoderSetViewport(Get(), x, y, width, height, minDepth, maxDepth);
}
void RenderPassEncoder::WriteTimestamp(QuerySet const& querySet, uint32_t queryIndex) const {
    wgpuRenderPassEncoderWriteTimestamp(Get(), querySet.Get(), queryIndex);
}


void RenderPassEncoder::WGPUAddRef(WGPURenderPassEncoder handle) {
    if (handle != nullptr) {
        wgpuRenderPassEncoderAddRef(handle);
    }
}
void RenderPassEncoder::WGPURelease(WGPURenderPassEncoder handle) {
    if (handle != nullptr) {
        wgpuRenderPassEncoderRelease(handle);
    }
}
static_assert(sizeof(RenderPassEncoder) == sizeof(WGPURenderPassEncoder), "sizeof mismatch for RenderPassEncoder");
static_assert(alignof(RenderPassEncoder) == alignof(WGPURenderPassEncoder), "alignof mismatch for RenderPassEncoder");

// RenderPipeline implementation

BindGroupLayout RenderPipeline::GetBindGroupLayout(uint32_t groupIndex) const {
    auto result = wgpuRenderPipelineGetBindGroupLayout(Get(), groupIndex);
    return BindGroupLayout::Acquire(result);
}
void RenderPipeline::SetLabel(StringView label) const {
    wgpuRenderPipelineSetLabel(Get(), *reinterpret_cast<WGPUStringView const*>(&label));
}


void RenderPipeline::WGPUAddRef(WGPURenderPipeline handle) {
    if (handle != nullptr) {
        wgpuRenderPipelineAddRef(handle);
    }
}
void RenderPipeline::WGPURelease(WGPURenderPipeline handle) {
    if (handle != nullptr) {
        wgpuRenderPipelineRelease(handle);
    }
}
static_assert(sizeof(RenderPipeline) == sizeof(WGPURenderPipeline), "sizeof mismatch for RenderPipeline");
static_assert(alignof(RenderPipeline) == alignof(WGPURenderPipeline), "alignof mismatch for RenderPipeline");

// Sampler implementation

void Sampler::SetLabel(StringView label) const {
    wgpuSamplerSetLabel(Get(), *reinterpret_cast<WGPUStringView const*>(&label));
}


void Sampler::WGPUAddRef(WGPUSampler handle) {
    if (handle != nullptr) {
        wgpuSamplerAddRef(handle);
    }
}
void Sampler::WGPURelease(WGPUSampler handle) {
    if (handle != nullptr) {
        wgpuSamplerRelease(handle);
    }
}
static_assert(sizeof(Sampler) == sizeof(WGPUSampler), "sizeof mismatch for Sampler");
static_assert(alignof(Sampler) == alignof(WGPUSampler), "alignof mismatch for Sampler");

// ShaderModule implementation

template <typename F, typename T,
          typename Cb,
          typename CbChar,
          typename>
Future ShaderModule::GetCompilationInfo(CallbackMode callbackMode,F callback, T userdata) const {
    WGPUCompilationInfoCallbackInfo callbackInfo = {};
    callbackInfo.mode = static_cast<WGPUCallbackMode>(callbackMode);
    if constexpr (std::is_convertible_v<F, Cb*>) {
        callbackInfo.callback = [](WGPUCompilationInfoRequestStatus status, WGPUCompilationInfo const * compilationInfo, void* callback_param, void* userdata_param) {
            auto cb = reinterpret_cast<Cb*>(callback_param);
            (*cb)(static_cast<CompilationInfoRequestStatus>(status), reinterpret_cast<CompilationInfo const*>(compilationInfo), static_cast<T>(userdata_param));
        };
    } else {
        callbackInfo.callback = [](WGPUCompilationInfoRequestStatus status, WGPUCompilationInfo const * compilationInfo, void* callback_param, void* userdata_param) {
            auto cb = reinterpret_cast<CbChar*>(callback_param);
            (*cb)(static_cast<CompilationInfoRequestStatus>(status), reinterpret_cast<CompilationInfo const*>(compilationInfo), static_cast<T>(userdata_param));
        };
    }
    callbackInfo.userdata1 = reinterpret_cast<void*>(+callback);
    callbackInfo.userdata2 = reinterpret_cast<void*>(userdata);
    auto result = wgpuShaderModuleGetCompilationInfo(Get(), callbackInfo);
    return Future {
        result.id
    };
}
template <typename L,
          typename Cb,
          typename CbChar,
          typename>
Future ShaderModule::GetCompilationInfo(CallbackMode callbackMode,L callback) const {
    using F = CompilationInfoCallback<void>;

    WGPUCompilationInfoCallbackInfo callbackInfo = {};
    callbackInfo.mode = static_cast<WGPUCallbackMode>(callbackMode);
    if constexpr (std::is_convertible_v<L, F*>) {
        callbackInfo.callback = [](WGPUCompilationInfoRequestStatus status, WGPUCompilationInfo const * compilationInfo, void* callback_param, void*) {
            auto cb = reinterpret_cast<F*>(callback_param);
            (*cb)(static_cast<CompilationInfoRequestStatus>(status), reinterpret_cast<CompilationInfo const*>(compilationInfo));
        };
        callbackInfo.userdata1 = reinterpret_cast<void*>(+callback);
        callbackInfo.userdata2 = nullptr;
    } else {
        auto* lambda = new L(std::move(callback));
        callbackInfo.callback = [](WGPUCompilationInfoRequestStatus status, WGPUCompilationInfo const * compilationInfo, void* callback_param, void*) {
            std::unique_ptr<L> the_lambda(reinterpret_cast<L*>(callback_param));
            (*the_lambda)(static_cast<CompilationInfoRequestStatus>(status), reinterpret_cast<CompilationInfo const*>(compilationInfo));
        };
        callbackInfo.userdata1 = reinterpret_cast<void*>(lambda);
        callbackInfo.userdata2 = nullptr;
    }
    auto result = wgpuShaderModuleGetCompilationInfo(Get(), callbackInfo);
    return Future {
            result.id
        };
}
void ShaderModule::SetLabel(StringView label) const {
    wgpuShaderModuleSetLabel(Get(), *reinterpret_cast<WGPUStringView const*>(&label));
}


void ShaderModule::WGPUAddRef(WGPUShaderModule handle) {
    if (handle != nullptr) {
        wgpuShaderModuleAddRef(handle);
    }
}
void ShaderModule::WGPURelease(WGPUShaderModule handle) {
    if (handle != nullptr) {
        wgpuShaderModuleRelease(handle);
    }
}
static_assert(sizeof(ShaderModule) == sizeof(WGPUShaderModule), "sizeof mismatch for ShaderModule");
static_assert(alignof(ShaderModule) == alignof(WGPUShaderModule), "alignof mismatch for ShaderModule");

// Surface implementation

void Surface::Configure(SurfaceConfiguration const * config) const {
    wgpuSurfaceConfigure(Get(), reinterpret_cast<WGPUSurfaceConfiguration const *>(config));
}
ConvertibleStatus Surface::GetCapabilities(Adapter const& adapter, SurfaceCapabilities * capabilities) const {
    *capabilities = SurfaceCapabilities();
    auto result = wgpuSurfaceGetCapabilities(Get(), adapter.Get(), reinterpret_cast<WGPUSurfaceCapabilities *>(capabilities));
    return static_cast<Status>(result);
}
void Surface::GetCurrentTexture(SurfaceTexture * surfaceTexture) const {
    wgpuSurfaceGetCurrentTexture(Get(), reinterpret_cast<WGPUSurfaceTexture *>(surfaceTexture));
}
ConvertibleStatus Surface::Present() const {
    auto result = wgpuSurfacePresent(Get());
    return static_cast<Status>(result);
}
void Surface::SetLabel(StringView label) const {
    wgpuSurfaceSetLabel(Get(), *reinterpret_cast<WGPUStringView const*>(&label));
}
void Surface::Unconfigure() const {
    wgpuSurfaceUnconfigure(Get());
}


void Surface::WGPUAddRef(WGPUSurface handle) {
    if (handle != nullptr) {
        wgpuSurfaceAddRef(handle);
    }
}
void Surface::WGPURelease(WGPUSurface handle) {
    if (handle != nullptr) {
        wgpuSurfaceRelease(handle);
    }
}
static_assert(sizeof(Surface) == sizeof(WGPUSurface), "sizeof mismatch for Surface");
static_assert(alignof(Surface) == alignof(WGPUSurface), "alignof mismatch for Surface");

// Texture implementation

TextureView Texture::CreateView(TextureViewDescriptor const * descriptor) const {
    auto result = wgpuTextureCreateView(Get(), reinterpret_cast<WGPUTextureViewDescriptor const *>(descriptor));
    return TextureView::Acquire(result);
}
void Texture::Destroy() const {
    wgpuTextureDestroy(Get());
}
uint32_t Texture::GetDepthOrArrayLayers() const {
    auto result = wgpuTextureGetDepthOrArrayLayers(Get());
    return result;
}
TextureDimension Texture::GetDimension() const {
    auto result = wgpuTextureGetDimension(Get());
    return static_cast<TextureDimension>(result);
}
TextureFormat Texture::GetFormat() const {
    auto result = wgpuTextureGetFormat(Get());
    return static_cast<TextureFormat>(result);
}
uint32_t Texture::GetHeight() const {
    auto result = wgpuTextureGetHeight(Get());
    return result;
}
uint32_t Texture::GetMipLevelCount() const {
    auto result = wgpuTextureGetMipLevelCount(Get());
    return result;
}
uint32_t Texture::GetSampleCount() const {
    auto result = wgpuTextureGetSampleCount(Get());
    return result;
}
TextureUsage Texture::GetUsage() const {
    auto result = wgpuTextureGetUsage(Get());
    return static_cast<TextureUsage>(result);
}
uint32_t Texture::GetWidth() const {
    auto result = wgpuTextureGetWidth(Get());
    return result;
}
void Texture::SetLabel(StringView label) const {
    wgpuTextureSetLabel(Get(), *reinterpret_cast<WGPUStringView const*>(&label));
}


void Texture::WGPUAddRef(WGPUTexture handle) {
    if (handle != nullptr) {
        wgpuTextureAddRef(handle);
    }
}
void Texture::WGPURelease(WGPUTexture handle) {
    if (handle != nullptr) {
        wgpuTextureRelease(handle);
    }
}
static_assert(sizeof(Texture) == sizeof(WGPUTexture), "sizeof mismatch for Texture");
static_assert(alignof(Texture) == alignof(WGPUTexture), "alignof mismatch for Texture");

// TextureView implementation

void TextureView::SetLabel(StringView label) const {
    wgpuTextureViewSetLabel(Get(), *reinterpret_cast<WGPUStringView const*>(&label));
}


void TextureView::WGPUAddRef(WGPUTextureView handle) {
    if (handle != nullptr) {
        wgpuTextureViewAddRef(handle);
    }
}
void TextureView::WGPURelease(WGPUTextureView handle) {
    if (handle != nullptr) {
        wgpuTextureViewRelease(handle);
    }
}
static_assert(sizeof(TextureView) == sizeof(WGPUTextureView), "sizeof mismatch for TextureView");
static_assert(alignof(TextureView) == alignof(WGPUTextureView), "alignof mismatch for TextureView");




// Free Functions
static inline Instance CreateInstance(InstanceDescriptor const * descriptor = nullptr) {
    auto result = wgpuCreateInstance(reinterpret_cast<WGPUInstanceDescriptor const *>(descriptor));
    return Instance::Acquire(result);
}
static inline void GetInstanceFeatures(SupportedInstanceFeatures * features) {
    wgpuGetInstanceFeatures(reinterpret_cast<WGPUSupportedInstanceFeatures *>(features));
}
static inline Status GetInstanceLimits(InstanceLimits * limits) {
    auto result = wgpuGetInstanceLimits(reinterpret_cast<WGPUInstanceLimits *>(limits));
    return static_cast<Status>(result);
}
static inline Bool HasInstanceFeature(InstanceFeatureName feature) {
    auto result = wgpuHasInstanceFeature(static_cast<WGPUInstanceFeatureName>(feature));
    return result;
}
static inline Proc GetProcAddress(StringView procName) {
    auto result = wgpuGetProcAddress(*reinterpret_cast<WGPUStringView const*>(&procName));
    return reinterpret_cast<Proc>(result);
}

}  // namespace wgpu

namespace wgpu {

template<>
struct IsWGPUBitmask<wgpu::BufferUsage> {
    static constexpr bool enable = true;
};

template<>
struct IsWGPUBitmask<wgpu::ColorWriteMask> {
    static constexpr bool enable = true;
};

template<>
struct IsWGPUBitmask<wgpu::MapMode> {
    static constexpr bool enable = true;
};

template<>
struct IsWGPUBitmask<wgpu::ShaderStage> {
    static constexpr bool enable = true;
};

template<>
struct IsWGPUBitmask<wgpu::TextureUsage> {
    static constexpr bool enable = true;
};


inline bool operator==(const TextureComponentSwizzle& s1, const TextureComponentSwizzle& s2) {
    return s1.r == s2.r && s1.g == s2.g && s1.b == s2.b && s1.a == s2.a;
}
inline bool operator!=(const TextureComponentSwizzle& s1, const TextureComponentSwizzle& s2) {
    return !(s1 == s2);
}

} // namespace wgpu

namespace std {
// Custom boolean class needs corresponding hash function so that it appears as a transparent bool.
template <>
struct hash<wgpu::Bool> {
  public:
    size_t operator()(const wgpu::Bool &v) const {
        return hash<bool>()(v);
    }
};
template <>
struct hash<wgpu::OptionalBool> {
  public:
    size_t operator()(const wgpu::OptionalBool &v) const {
        return hash<WGPUOptionalBool>()(v.mValue);
    }
};
}  // namespace std

#endif // WEBGPU_CPP_H_
