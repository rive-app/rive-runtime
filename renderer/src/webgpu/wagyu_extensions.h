#ifndef WAGYU_EXTENSIONS_H
#define WAGYU_EXTENSIONS_H

#include "webgpu/webgpu.h"

#if defined(__cplusplus)
#if __cplusplus >= 201103L
#define WAGYU_MAKE_INIT_STRUCT(type, value) (type value)
#else
#define WAGYU_MAKE_INIT_STRUCT(type, value) value
#endif
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define WAGYU_MAKE_INIT_STRUCT(type, value) ((type)value)
#else
#define WAGYU_MAKE_INIT_STRUCT(type, value) value
#endif

#define WAGYU_CHAIN_INIT(sType)                                                \
    WAGYU_MAKE_INIT_STRUCT(WGPUChainedStruct,                                  \
                           {/*.next = */ NULL WGPU_COMMA /*.sType = */ (       \
                               WGPUSType) sType WGPU_COMMA})

// this should be equivalent to the type of the same name in the new webgpu.h
#if !defined(WGPUOptionalBool)
#define WGPUOptionalBool int32_t
#define WGPUOptionalBool_False 0x00000000
#define WGPUOptionalBool_True 0x00000001
#define WGPUOptionalBool_Undefined 0x00000002
#define WGPUOptionalBool_Force32 0x7FFFFFFF
#endif

#define WAGYU_STRLEN SIZE_MAX

#if defined(USE_WAGYU_NAMESPACE) || defined(__cppcheck)
namespace wagyu1
{
#endif

typedef struct WagyuRelaxedComplianceImpl* WagyuRelaxedCompliance
    WGPU_OBJECT_ATTRIBUTE;
typedef struct WagyuExternalTextureImpl* WagyuExternalTexture
    WGPU_OBJECT_ATTRIBUTE;

typedef enum WagyuDeviceFlushStatus
{
    WagyuDeviceFlushStatus_Success = 0x00000000,
    WagyuDeviceFlushStatus_Error = 0x00000001,
    WagyuDeviceFlushStatus_Force32 = 0x7FFFFFFF
} WagyuDeviceFlushStatus WGPU_ENUM_ATTRIBUTE;

typedef enum WagyuSType
{
    // reserved range for wagyu (Netflix) starts at 0x00060000
    WagyuSType_ExternalTextureBindingEntry = 0x00060000,
    WagyuSType_ExternalTextureBindingLayout = 0x00060001,
    WagyuSType_InputTextureBindingLayout = 0x00060002, // FIX - is this needed?
    WagyuSType_ColorTargetState = 0x00060003,
    WagyuSType_FragmentState = 0x00060004,
    WagyuSType_TextureDescriptor = 0x00060005,
    WagyuSType_ShaderModuleDescriptor = 0x00060006,
    WagyuSType_CommandEncoderDescriptor = 0x00060007,
    WagyuSType_ComputePipelineDescriptor = 0x00060008,
    WagyuSType_DeviceDescriptor = 0x00060009,
    WagyuSType_RenderPipelineDescriptor = 0x0006000A,
    WagyuSType_RenderPassDescriptor = 0x0006000B,
    WagyuSType_SurfaceConfiguration = 0x0006000C,
    WagyuSType_Force32 = 0x7FFFFFFF
} WagyuSType WGPU_ENUM_ATTRIBUTE;

typedef enum WagyuPredefinedColorSpace
{
    WagyuPredefinedColorSpace_SRGB = 0x00000001,
    WagyuPredefinedColorSpace_DisplayP3 = 0x00000002,
    WagyuPredefinedColorSpace_Force32 = 0x7FFFFFFF
} WagyuPredefinedColorSpace WGPU_ENUM_ATTRIBUTE;

typedef enum WagyuShaderLanguage
{
    WagyuShaderLanguage_Detect = 0x00000000,
    WagyuShaderLanguage_GLSL = 0x00000001,
    WagyuShaderLanguage_GLSLRAW = 0x00000002,
    WagyuShaderLanguage_WGSL = 0x00000003,
    WagyuShaderLanguage_SPIRV = 0x00000004,
    WagyuShaderLanguage_Force32 = 0x7FFFFFFF
} WagyuShaderLanguage WGPU_ENUM_ATTRIBUTE;

typedef enum WagyuWGSLFeatureType
{
    WagyuWGSLFeatureType_Testing = 0x00000000,
    WagyuWGSLFeatureType_UnsafeExperimental = 0x00000001,
    WagyuWGSLFeatureType_Experimental = 0x00000002,
    WagyuWGSLFeatureType_All = 0x00000003,
    WagyuWGSLFeatureType_Force32 = 0x7FFFFFFF
} WagyuWGSLFeatureType WGPU_ENUM_ATTRIBUTE;

typedef WGPUFlags WagyuFragmentStateFeaturesFlags;
static const WagyuFragmentStateFeaturesFlags
    WagyuFragmentStateFeaturesFlags_None = 0x00000000;
static const WagyuFragmentStateFeaturesFlags
    WagyuFragmentStateFeaturesFlags_RasterizationOrderAttachmentAccess =
        0x00000001;

static const WGPUTextureUsage WagyuTextureUsage_InputAttachment =
    (WGPUTextureUsage)(0x10000);
static const WGPUTextureUsage WagyuTextureUsage_TransientAttachment =
    (WGPUTextureUsage)(0x20000);

typedef void (*WagyuDeviceFlushCallback)(WGPUDevice device,
                                         WagyuDeviceFlushStatus status,
                                         void* userdata)
    WGPU_FUNCTION_ATTRIBUTE;

typedef struct WagyuStringView
{
    WGPU_NULLABLE const char* data;
    size_t length;
} WagyuStringView WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_STRING_VIEW_INIT                                                 \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuStringView,                                                       \
        {/*.data = */ NULL WGPU_COMMA /*.length = */ 0 WGPU_COMMA})

typedef struct WagyuColorTargetState
{
    WGPUChainedStruct chain;
    WGPUOptionalBool usedAsInput;
} WagyuColorTargetState WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_COLOR_TARGET_STATE_INIT                                          \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuColorTargetState,                                                 \
        {/*.chain*/ WAGYU_CHAIN_INIT(WagyuSType_ColorTargetState)              \
             WGPU_COMMA /*.usedAsInput*/ WGPUOptionalBool_Undefined            \
                 WGPU_COMMA})

typedef struct WagyuCommandEncoderDescriptor
{
    WGPUChainedStruct chain;
    WGPUOptionalBool measureExecutionTime;
} WagyuCommandEncoderDescriptor WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_COMMAND_ENCODER_DESCRIPTOR_INIT                                  \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuCommandEncoderDescriptor,                                         \
        {/*.chain*/ WAGYU_CHAIN_INIT(WagyuSType_CommandEncoderDescriptor)      \
             WGPU_COMMA /*.measureExecutionTime*/ WGPUOptionalBool_Undefined   \
                 WGPU_COMMA})

typedef struct WagyuComputePipelineDescriptor
{
    WGPUChainedStruct chain;
    WagyuStringView cacheKey;
} WagyuComputePipelineDescriptor WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_COMPUTE_PIPELINE_DESCRIPTOR_INIT                                 \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuComputePipelineDescriptor,                                        \
        {/*.chain*/ WAGYU_CHAIN_INIT(WagyuSType_ComputePipelineDescriptor)     \
             WGPU_COMMA /*.cacheKey*/ WAGYU_STRING_VIEW_INIT WGPU_COMMA})

typedef struct WagyuDeviceDescriptor
{
    WGPUChainedStruct chain;
    WGPUBool dataBufferNeedsDetach;
} WagyuDeviceDescriptor WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_DEVICE_DESCRIPTOR_INIT                                           \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuDeviceDescriptor,                                                 \
        {/*.chain*/ WAGYU_CHAIN_INIT(WagyuSType_DeviceDescriptor)              \
             WGPU_COMMA /*.dataBufferNeedsDetach*/ false WGPU_COMMA})

typedef struct WagyuDeviceFlushCallbackInfo
{
    WGPUChainedStruct* nextInChain;
    WGPUCallbackMode mode;
    WagyuDeviceFlushCallback callback;
    void* userdata;
} WagyuDeviceFlushCallbackInfo WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_DEVICE_FLUSHCALLBACK_INFO_INIT                                   \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuDeviceFlushCallbackInfo,                                          \
        {/*.nextInChain = */ NULL WGPU_COMMA              /*.mode = */         \
             WGPUCallbackMode_AllowSpontaneous WGPU_COMMA /*.callback = */     \
                 NULL WGPU_COMMA /*.userdata1 = */ NULL                        \
                     WGPU_COMMA /*.userdata2 = */ NULL WGPU_COMMA})

typedef struct WagyuDevicePipelineBinary
{
    size_t binarySize;
    void* binary;
    size_t blobKeySize;
    void* blobKey;
} WagyuDevicePipelineBinary WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_DEVICE_PIPELINE_BINARY_INIT                                      \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuDevicePipelineBinary,                                             \
        {/*.binarySize*/ 0 WGPU_COMMA /*.binary*/ NULL                         \
             WGPU_COMMA /*.blobKeySize*/ 0 WGPU_COMMA /*.blobKey*/ NULL        \
                 WGPU_COMMA})

typedef struct WagyuDevicePipelineBinaryBlobKey
{
    size_t size;
    const void* data;
} WagyuDevicePipelineBinaryBlobKey WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_DEVICE_PIPELINE_BINARY_BLOB_KEY_INIT                             \
    WAGYU_MAKE_INIT_STRUCT(WagyuDevicePipelineBinaryBlobKey,                   \
                           {/*.size*/ 0 WGPU_COMMA /*.data*/ NULL WGPU_COMMA})

typedef struct WagyuDevicePipelineBinaryCacheKey
{
    WagyuStringView cacheKey;
    size_t blobKeysLength;
    WagyuDevicePipelineBinaryBlobKey* blobKeys;
} WagyuDevicePipelineBinaryCacheKey WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_DEVICE_PIPELINE_BINARY_CACHE_KEY_INIT                            \
    WAGYU_MAKE_INIT_STRUCT(WagyuDevicePipelineBinaryCacheKey,                  \
                           {/*.cacheKey*/ WAGYU_STRING_VIEW_INIT               \
                                WGPU_COMMA /*.blobKeysLength*/ 0 WGPU_COMMA})

typedef struct WagyuDevicePipelineBinaryData
{
    size_t binariesLength;
    WagyuDevicePipelineBinary* binaries;
    size_t cacheKeysLength;
    WagyuDevicePipelineBinaryCacheKey* cacheKeys;
} WagyuDevicePipelineBinaryData WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_DEVICE_PIPELINE_BINARY_DATA_INIT                                 \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuDevicePipelineBinaryData,                                         \
        {/*.binariesLength*/ 0 WGPU_COMMA /*.binaries*/ NULL                   \
             WGPU_COMMA /*.cacheKeysLength*/ 0 WGPU_COMMA /*.cacheKeys*/ NULL  \
                 WGPU_COMMA})

typedef struct WagyuExternalTextureDescriptor
{
    const WGPUChainedStruct* nextInChain;
    WagyuStringView label;
    WagyuStringView source;
    WagyuPredefinedColorSpace colorSpace;
} WagyuExternalTextureDescriptor WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_EXTERNAL_TEXTURE_DESCRIPTOR_INIT                                 \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuExternalTextureDescriptor,                                        \
        {/*nextInChain = */ NULL WGPU_COMMA        /*label = */                \
             WAGYU_STRING_VIEW_INIT WGPU_COMMA     /*source = */               \
                 WAGYU_STRING_VIEW_INIT WGPU_COMMA /*colorSpace = */           \
                     WagyuPredefinedColorSpace_SRGB WGPU_COMMA})

typedef struct WagyuExternalTextureBindingEntry
{
    WGPUChainedStruct chain;
    WagyuExternalTexture externalTexture;
} WagyuExternalTextureBindingEntry WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_EXTERNAL_TEXTURE_BINDING_ENTRY_INIT                              \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuExternalTextureBindingEntry,                                      \
        {/*.chain = */ WAGYU_CHAIN_INIT(                                       \
            WagyuSType_ExternalTextureBindingEntry)                            \
             WGPU_COMMA /*.externalTexture = */ NULL WGPU_COMMA})

typedef struct WagyuExternalTextureBindingLayout
{
    WGPUChainedStruct chain;
} WagyuExternalTextureBindingLayout WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_EXTERNAL_TEXTURE_BINDING_LAYOUT_INIT                             \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuExternalTextureBindingLayout,                                     \
        {/*.chain*/ WAGYU_CHAIN_INIT(WagyuSType_ExternalTextureBindingLayout)  \
             WGPU_COMMA})

typedef struct WagyuFragmentState
{
    WGPUChainedStruct chain;
    WagyuFragmentStateFeaturesFlags featureFlags;
} WagyuFragmentState WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_FRAGMENT_STATE_INIT                                              \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuFragmentState,                                                    \
        {/*.chain*/ WAGYU_CHAIN_INIT(WagyuSType_FragmentState)                 \
             WGPU_COMMA /*.featureFlags*/ WagyuFragmentStateFeaturesFlags_None \
                 WGPU_COMMA})

typedef struct WagyuInputTextureBindingLayout
{
    WGPUChainedStruct chain;
    WGPUTextureViewDimension viewDimension;
} WagyuInputTextureBindingLayout WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_INPUT_TEXTURE_BINDING_LAYOUT_INIT                                \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuInputTextureBindingLayout,                                        \
        {/*.chain*/ WAGYU_CHAIN_INIT(WagyuSType_InputTextureBindingLayout)     \
             WGPU_COMMA /*.viewDimension*/ WGPUTextureViewDimension_2D         \
                 WGPU_COMMA})

typedef struct WagyuRect
{
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
} WagyuRect WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_RECT_INIT                                                        \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuRect,                                                             \
        {/* .x */ 0 WGPU_COMMA /* .y */ 0 WGPU_COMMA /* .width */              \
         0 WGPU_COMMA /* .height */ 0 WGPU_COMMA})

typedef struct WagyuRenderPassInputAttachment
{
    WGPUTextureView view;
    WGPU_NULLABLE WGPUColor* clearValue;
    WGPULoadOp loadOp;
    WGPUStoreOp storeOp;
} WagyuRenderPassInputAttachment WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_RENDER_PASS_INPUT_ATTACHMENT_INIT                                \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuRenderPassInputAttachment,                                        \
        {/* .view */ NULL WGPU_COMMA /* .clearValue */ NULL                    \
             WGPU_COMMA /* .loadOp */ WGPULoadOp_Clear                         \
                 WGPU_COMMA /* .storeOp */ WGPUStoreOp_Store WGPU_COMMA})

typedef struct WagyuRenderPassDescriptor
{
    WGPUChainedStruct chain;
    size_t inputAttachmentCount;
    WGPU_NULLABLE WagyuRenderPassInputAttachment* inputAttachments;
} WagyuRenderPassDescriptor WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_RENDER_PASS_DESCRIPTOR_INIT                                      \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuRenderPassDescriptor,                                             \
        {/* .chain */ WAGYU_CHAIN_INIT(WagyuSType_RenderPassDescriptor)        \
             WGPU_COMMA /* .inputAttachmentCount */                            \
         0 WGPU_COMMA /* .inputAttachments */ NULL WGPU_COMMA})

typedef struct WagyuRenderPipelineDescriptor
{
    WGPUChainedStruct chain;
    WagyuStringView cacheKey;
} WagyuRenderPipelineDescriptor WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_RENDER_PIPELINE_DESCRIPTOR_INIT                                  \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuRenderPipelineDescriptor,                                         \
        {/* .chain */ WAGYU_CHAIN_INIT(WagyuSType_RenderPipelineDescriptor)    \
             WGPU_COMMA /* .cacheKey */ WAGYU_STRING_VIEW_INIT WGPU_COMMA})

typedef struct WagyuShaderReflectionStructMember
{
    WagyuStringView name;
    uint32_t group;
    uint32_t binding;
    uint32_t offset;
    uint32_t size;
    uint32_t type;
    WGPUBool imageMultisampled;
    WGPUTextureViewDimension imageDimension;
    WGPUTextureFormat imageFormat;
} WagyuShaderReflectionStructMember WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_SHADER_REFLECTION_STRUCT_MEMBER_INIT                             \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuShaderReflectionStructMember,                                     \
        {/* .name */ WAGYU_STRING_VIEW_INIT                                    \
             WGPU_COMMA /* .group */ 0 WGPU_COMMA /* .binding */               \
         0 WGPU_COMMA /* .offset */ 0 WGPU_COMMA  /* .size */                  \
         0 WGPU_COMMA /* .type */ 0 WGPU_COMMA    /* .imageMultisampled */     \
         false WGPU_COMMA                         /* .imageDimension */        \
             WGPUTextureViewDimension_Undefined WGPU_COMMA /* .imageFormat */  \
                 WGPUTextureFormat_Undefined WGPU_COMMA})

typedef struct WagyuShaderReflectionLocation
{
    WagyuStringView name;
    uint32_t location;
    uint32_t size;
    uint32_t type;
} WagyuShaderReflectionLocation WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_SHADER_REFLECTION_LOCATION_INIT                                  \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuShaderReflectionLocation,                                         \
        {/* .name */ WAGYU_STRING_VIEW_INIT WGPU_COMMA /* .location */         \
         0 WGPU_COMMA /* .size */ 0 WGPU_COMMA /* .type */ 0 WGPU_COMMA})

typedef struct WagyuShaderReflectionResource
{
    WagyuStringView name;
    uint32_t group;
    uint32_t binding;
    uint32_t bindingType;
    WGPUBool multisampled;
    WGPUTextureViewDimension dimension;
    WGPUTextureFormat format;
    uint64_t bufferSize;
} WagyuShaderReflectionResource WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_SHADER_REFLECTION_RESOURCE_INIT                                  \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuShaderReflectionResource,                                         \
        {/* .name */ WAGYU_STRING_VIEW_INIT WGPU_COMMA     /* .group */        \
         0 WGPU_COMMA /* .binding */ 0 WGPU_COMMA          /* .bindingType */  \
         0 WGPU_COMMA /* .multisampled */ false WGPU_COMMA /* .dimension */    \
             WGPUTextureViewDimension_Undefined                                \
                 WGPU_COMMA /* .format */ WGPUTextureFormat_Undefined          \
                     WGPU_COMMA /* .bufferSize */ 0 WGPU_COMMA})

typedef struct WagyuShaderReflectionSpecializationConstant
{
    uint32_t id;
    uint32_t internalId;
    uint32_t type;
    WagyuStringView name;
} WagyuShaderReflectionSpecializationConstant WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_SHADER_REFLECTION_SPECIALIZATION_CONSTANT_INIT                   \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuShaderReflectionSpecializationConstant,                           \
        {/* .id */ 0 WGPU_COMMA /* .internalId */ 0 WGPU_COMMA /* .type */     \
         0 WGPU_COMMA /* .name */ WAGYU_STRING_VIEW_INIT WGPU_COMMA})

typedef struct WagyuShaderReflectionData
{
    size_t resourcesCount;
    WagyuShaderReflectionResource* resources;
    size_t constantsCount;
    WagyuShaderReflectionSpecializationConstant* constants;
    size_t uniformsCount;
    WagyuShaderReflectionStructMember* uniforms;
    size_t attributesCount;
    WagyuShaderReflectionLocation* attributes;
    WagyuStringView wgsl;
} WagyuShaderReflectionData WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_SHADER_REFLECTION_DATA_INIT                                      \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuShaderReflectionData,                                             \
        {/* .resourcesCount */ 0 WGPU_COMMA /* .resources */ NULL              \
             WGPU_COMMA /* .constantsCount */ 0 WGPU_COMMA /* .constants */    \
                 NULL WGPU_COMMA /* .uniformsCount */                          \
         0 WGPU_COMMA /* .uniforms */ NULL                                     \
             WGPU_COMMA /* .attributesCount */ 0 WGPU_COMMA /* .attributes */  \
                 NULL WGPU_COMMA /* .wgsl */ WAGYU_STRING_VIEW_INIT            \
                     WGPU_COMMA})

typedef struct WagyuShaderEntryPoint
{
    WagyuStringView entryPoint;
    WGPUShaderStage stage;
    WagyuShaderReflectionData reflection;
} WagyuShaderEntryPoint WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_SHADER_ENTRY_POINT_INIT                                          \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuShaderEntryPoint,                                                 \
        {/* .entryPoint */ WAGYU_STRING_VIEW_INIT WGPU_COMMA /* .stage */      \
             WGPUShaderStage_NONE WGPU_COMMA                 /* .reflection */ \
                 WAGYU_SHADER_REFLECTION_DATA_INIT WGPU_COMMA})

typedef struct WagyuShaderEntryPointArray
{
    size_t entryPointsCount;
    WagyuShaderEntryPoint* entryPoints;
} WagyuShaderEntryPointArray WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_SHADER_ENTRY_POINT_ARRAY_INIT                                    \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuShaderEntryPointArray,                                            \
        {/* .entryPointsCount */ 0 WGPU_COMMA /* .entryPoints */ NULL          \
             WGPU_COMMA})

struct WagyuShaderModuleCompilationHint
{
    WGPUChainedStruct* nextInChain;
    WagyuStringView entryPoint;
    /**
     * If set to NULL, it will be treated as "auto"
     */
    WGPUPipelineLayout layout;
};

#define WAGYU_SHADER_MODULE_COMPILATION_HINT_INIT                              \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuShaderModuleCompilationHint,                                      \
        {/* .nextInChain */ NULL WGPU_COMMA /* .entryPoint */                  \
             WAGYU_STRING_VIEW_INIT WGPU_COMMA /* .layout */ NULL WGPU_COMMA})

typedef struct WagyuShaderModuleDescriptor
{
    WGPUChainedStruct chain;
    size_t codeSize; // bytes
    const void* code;
    WagyuShaderLanguage language;
    size_t compilationHintCount;
    const struct WagyuShaderModuleCompilationHint* compilationHints;
} WagyuShaderModuleDescriptor WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_SHADER_MODULE_DESCRIPTOR_INIT                                    \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuShaderModuleDescriptor,                                           \
        {/*.chain=*/WAGYU_CHAIN_INIT(WagyuSType_ShaderModuleDescriptor)        \
             WGPU_COMMA /*.codeSize*/ 0 WGPU_COMMA /*.code*/ NULL              \
                 WGPU_COMMA /*.language*/ WagyuShaderLanguage_Detect           \
                     WGPU_COMMA /*.compilationHintCount*/                      \
         0 WGPU_COMMA /*.compilationHints*/ NULL WGPU_COMMA})

typedef struct WagyuStringArray
{
    size_t stringCount;
    WGPU_NULLABLE WagyuStringView* strings;
} WagyuStringArray WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_STRING_ARRAY_INIT                                                \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuStringArray,                                                      \
        {/* .stringCount */ 0 WGPU_COMMA /* .strings */ NULL WGPU_COMMA})

typedef struct WagyuSurfaceConfiguration
{
    WGPUChainedStruct chain;
    int32_t* indirectRenderTargets;
} WagyuSurfaceConfiguration WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_SURFACE_CONFIGURATION_INIT                                       \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuSurfaceConfiguration,                                             \
        {/*.chain=*/WAGYU_CHAIN_INIT(WagyuSType_SurfaceConfiguration)          \
             WGPU_COMMA /*.indirectRenderTargets*/ NULL WGPU_COMMA})

typedef struct WagyuTextureDescriptor
{
    WGPUChainedStruct chain;
    WGPUBool useSurfaceCache;
} WagyuTextureDescriptor WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_TEXTURE_DESCRIPTPR_INIT                                          \
    WagyuTextureDescriptor                                                     \
    {                                                                          \
        .chain = WAGYU_CHAIN_INIT(WagyuSType_TextureDescriptor),               \
        .useSurfaceCache = 1,                                                  \
    }

typedef struct WagyuWGSLFeatureTypeArray
{
    size_t featuresCount;
    WGPU_NULLABLE WagyuWGSLFeatureType* features;
} WagyuWGSLFeatureTypeArray WGPU_STRUCTURE_ATTRIBUTE;

#define WAGYU_WGSL_FEATURE_TYPE_ARRAY_INIT                                     \
    WAGYU_MAKE_INIT_STRUCT(                                                    \
        WagyuWGSLFeatureTypeArray,                                             \
        {/* .featuresCount */ 0 WGPU_COMMA /* .features */ NULL WGPU_COMMA})

#if defined(__cplusplus) && !defined(USE_WAGYU_NAMESPACE) &&                   \
    !defined(__cppcheck)
extern "C"
{
#endif

    WGPU_EXPORT WGPUBackendType wagyuAdapterGetBackend(WGPUAdapter adapter)
        WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuAdapterGetExtensions(WGPUAdapter adapter,
                                               WagyuStringArray* extensions)
        WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuAdapterGetGraphicsReport(
        WGPUAdapter adapter,
        WagyuStringView* graphicsReport) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuAdapterGetName(WGPUAdapter adapter,
                                         WagyuStringView* name)
        WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT WGPUDevice wagyuAdapterRequestDeviceSync(
        WGPUAdapter adapter,
        WGPU_NULLABLE const WGPUDeviceDescriptor* options)
        WGPU_FUNCTION_ATTRIBUTE;

    WGPU_EXPORT void wagyuCommandEncoderBlit(
        WGPUCommandEncoder commandEncoder,
        const WGPUImageCopyTexture* source,
        const WGPUExtent3D* sourceExtent,
        const WGPUImageCopyTexture* destination,
        const WGPUExtent3D* destinationExtent,
        WGPUFilterMode filter) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuCommandEncoderGenerateMipmap(
        WGPUCommandEncoder commandEncoder,
        WGPUTexture texture) WGPU_FUNCTION_ATTRIBUTE;

    WGPU_EXPORT void wagyuDeviceClearPipelineBinaryCache(WGPUDevice device)
        WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuDeviceEnableImaginationWorkarounds(WGPUDevice device,
                                                             WGPUBool enable)
        WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuDeviceGetExtensions(WGPUDevice device,
                                              WagyuStringArray* extensions)
        WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuDeviceFlush(WGPUDevice device,
                                      WagyuDeviceFlushCallback callback,
                                      void* userdata) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT WagyuExternalTexture wagyuDeviceImportExternalTexture(
        WGPUDevice device,
        const WagyuExternalTextureDescriptor* descriptor)
        WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuDeviceIntrospectShaderCode(
        WGPUDevice device,
        WGPUShaderStage stages,
        const WGPUShaderModuleDescriptor* descriptor,
        WagyuShaderEntryPointArray* entryPoints) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuDevicePopulatePipelineBinaryCache(
        WGPUDevice device,
        const WagyuDevicePipelineBinaryData* data) WGPU_FUNCTION_ATTRIBUTE;

    WGPU_EXPORT void wagyuExternalTextureAddRef(
        WagyuExternalTexture externalTexture) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuExternalTextureRelease(
        WagyuExternalTexture externalTexture) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuExternalTextureSetLabel(
        WagyuExternalTexture externalTexture,
        const char* label) WGPU_FUNCTION_ATTRIBUTE;

    WGPU_EXPORT void wagyuInstanceEnableImaginationWorkarounds(
        WGPUInstance instance,
        WGPUBool enable) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT uint32_t wagyuInstanceGetApiVersion(WGPUInstance instance)
        WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT WGPUBackendType wagyuInstanceGetBackend(WGPUInstance instance)
        WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuInstanceGetExposeWGSLFeatures(
        WGPUInstance instance,
        WagyuWGSLFeatureTypeArray* wgslFeatures) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT WagyuRelaxedCompliance wagyuInstanceGetRelaxedCompliance(
        WGPUInstance instance) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT WGPUTextureFormat wagyuInstanceGetScreenDirectFormat(
        WGPUInstance instance) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT WGPUTextureFormat wagyuInstanceGetScreenIndirectFormat(
        WGPUInstance instance) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT WGPUBool wagyuInstanceGetSync(WGPUInstance instance)
        WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT WGPUAdapter wagyuInstanceRequestAdapterSync(
        WGPUInstance instance,
        WGPU_NULLABLE const WGPURequestAdapterOptions* options)
        WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuInstanceSetCommandBufferLimit(WGPUInstance instance,
                                                        uint32_t limit)
        WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuInstanceSetExposeWGSLFeatures(
        WGPUInstance instance,
        const WagyuWGSLFeatureTypeArray* wgslFeatures) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuInstanceSetImmediate(WGPUInstance instance,
                                               WGPUBool enabled)
        WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuInstanceSetRunBarriersOnIncoherent(
        WGPUInstance instance,
        WGPUBool run) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuInstanceSetStagingBufferCacheSize(
        WGPUInstance instance,
        uint32_t size) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuInstanceSetSync(WGPUInstance instance, WGPUBool sync)
        WGPU_FUNCTION_ATTRIBUTE;

    WGPU_EXPORT WGPUBool wagyuRelaxedComplianceGetBufferClear(
        WagyuRelaxedCompliance relaxedCompliance) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT WGPUBool wagyuRelaxedComplianceGetTextureClear(
        WagyuRelaxedCompliance relaxedCompliance) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuRelaxedComplianceSetAll(
        WagyuRelaxedCompliance relaxedCompliance,
        WGPUBool enable) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuRelaxedComplianceSetBufferClear(
        WagyuRelaxedCompliance relaxedCompliance,
        WGPUBool enable) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuRelaxedComplianceSetTextureClear(
        WagyuRelaxedCompliance relaxedCompliance,
        WGPUBool enable) WGPU_FUNCTION_ATTRIBUTE;

    WGPU_EXPORT void wagyuRenderBundleEncoderClearColorAttachments(
        WGPURenderBundleEncoder renderBundleEncoder,
        const WagyuRect* rect,
        uint32_t baseAttachment,
        uint32_t numAttachments,
        const WGPUColor* color,
        uint32_t baseArrayLayer,
        uint32_t layerCount) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuRenderBundleEncoderClearDepthAttachment(
        WGPURenderBundleEncoder renderBundleEncoder,
        const WagyuRect* rect,
        float depth,
        uint32_t baseArrayLayer,
        uint32_t layerCount) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuRenderBundleEncoderClearStencilAttachment(
        WGPURenderBundleEncoder renderBundleEncoder,
        const WagyuRect* rect,
        uint32_t stencil,
        uint32_t baseArrayLayer,
        uint32_t layerCount) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuRenderBundleEncoderSetScissorRect(
        WGPURenderBundleEncoder renderBundleEncoder,
        uint32_t x,
        uint32_t y,
        uint32_t width,
        uint32_t height) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuRenderBundleEncoderSetScissorRectIndirect(
        WGPURenderBundleEncoder renderBundleEncoder,
        uint64_t indirectOffset,
        const uint32_t* indirectBuffer,
        size_t indirectBufferCount) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuRenderBundleEncoderSetViewport(
        WGPURenderBundleEncoder renderBundleEncoder,
        float x,
        float y,
        float width,
        float height,
        float minDepth,
        float maxDepth) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuRenderBundleEncoderSetViewportWithDepthIndirect(
        WGPURenderBundleEncoder renderBundleEncoder,
        uint64_t indirectOffset,
        const float* indirectBuffer,
        size_t indirectBufferCount) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuRenderBundleEncoderSetViewportWithoutDepthIndirect(
        WGPURenderBundleEncoder renderBundleEncoder,
        uint64_t indirectOffset,
        const float* indirectBuffer,
        size_t indirectBufferCount) WGPU_FUNCTION_ATTRIBUTE;

    WGPU_EXPORT void wagyuRenderPassEncoderClearColorAttachments(
        WGPURenderPassEncoder renderPassEncoder,
        const WagyuRect* rect,
        uint32_t baseAttachment,
        uint32_t numAttachments,
        const WGPUColor* color,
        uint32_t baseArrayLayer,
        uint32_t layerCount) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuRenderPassEncoderClearDepthAttachment(
        WGPURenderPassEncoder renderPassEncoder,
        const WagyuRect* rect,
        float depth,
        uint32_t baseArrayLayer,
        uint32_t layerCount) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuRenderPassEncoderClearStencilAttachment(
        WGPURenderPassEncoder renderPassEncoder,
        const WagyuRect* rect,
        uint32_t stencil,
        uint32_t baseArrayLayer,
        uint32_t layerCount) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuRenderPassEncoderSetShaderPixelLocalStorageEnabled(
        WGPURenderPassEncoder renderPassEncoder,
        WGPUBool enabled) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuRenderPassEncoderExecuteBundle(
        WGPURenderPassEncoder renderPassEncoder,
        WGPURenderBundle bundle) WGPU_FUNCTION_ATTRIBUTE;

    WGPU_EXPORT void wagyuShaderEntryPointArrayFreeMembers(
        WagyuShaderEntryPointArray value) WGPU_FUNCTION_ATTRIBUTE;

    WGPU_EXPORT void wagyuShaderModuleDestroy(WGPUShaderModule shaderModule)
        WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuShaderModuleIntrospect(
        WGPUShaderModule shaderModule,
        WGPUShaderStage stages,
        WagyuShaderEntryPointArray* shaderEntryPointArray)
        WGPU_FUNCTION_ATTRIBUTE;

    WGPU_EXPORT void wagyuStringArrayFreeMembers(WagyuStringArray value)
        WGPU_FUNCTION_ATTRIBUTE;

    WGPU_EXPORT void wagyuStringViewFreeMembers(WagyuStringView value)
        WGPU_FUNCTION_ATTRIBUTE;

    WGPU_EXPORT void wagyuSurfaceDestroy(WGPUSurface surface)
        WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT WGPUTexture wagyuSurfaceGetCurrentDepthStencilTexture(
        WGPUSurface surface) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT float wagyuSurfaceGetHeight(WGPUSurface surface)
        WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT float wagyuSurfaceGetWidth(WGPUSurface surface)
        WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT float wagyuSurfaceGetX(WGPUSurface surface)
        WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT float wagyuSurfaceGetY(WGPUSurface surface)
        WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuSurfacePresent(
        WGPUSurface surface,
        WGPUTexture target /**| nrdp.gibbon.Surface*/) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuSurfaceSetHeight(WGPUSurface surface, float height)
        WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuSurfaceSetWidth(WGPUSurface surface,
                                          float width) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuSurfaceSetX(WGPUSurface surface,
                                      float x) WGPU_FUNCTION_ATTRIBUTE;
    WGPU_EXPORT void wagyuSurfaceSetY(WGPUSurface surface,
                                      float y) WGPU_FUNCTION_ATTRIBUTE;

    WGPU_EXPORT void wagyuWGSLFeatureTypeArrayFreeMembers(
        WagyuWGSLFeatureTypeArray value) WGPU_FUNCTION_ATTRIBUTE;

#if defined(__cplusplus) && !defined(USE_WAGYU_NAMESPACE) &&                   \
    !defined(__cppcheck)
} // extern "C"
#endif

#if defined(USE_WAGYU_NAMESPACE) || defined(__cppcheck)
} // namespace wagyu1
#endif

#endif /* WAGYU_EXTENSIONS_H */
