#ifndef WEBGPU_WAGYU_H
#define WEBGPU_WAGYU_H

#include "webgpu/webgpu.h"

#define WGPU_WAGYU_EXTENSION_LEVEL 1

// Reserved range for Wagyu starts at 0x00060000
// https://github.com/webgpu-native/webgpu-headers/blob/main/doc/articles/Extensions.md#registry-of-prefixes-and-enum-blocks
#define WGPU_WAGYU_RESERVED_RANGE_BASE 0x00060000

#define WGPU_WAGYU_MAKE_INIT_STRUCT _wgpu_MAKE_INIT_STRUCT

#define WGPU_WAGYU_CHAIN_INIT(sType) \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUChainedStruct, { /*.next = */ NULL _wgpu_COMMA /*.sType = */ (WGPUSType) sType _wgpu_COMMA })

#define WGPU_WAGYU_STRLEN SIZE_MAX
#define WGPU_WAGYU_PIXEL_LOCAL_STORAGE_SIZE_UNDEFINED UINT32_MAX

typedef struct WGPUWagyuExternalTextureImpl *WGPUWagyuExternalTexture WGPU_OBJECT_ATTRIBUTE;

// These values extend the WGPUSType enum set from webgpu.h
typedef enum WGPUSType_Wagyu
{
    WGPUSType_WagyuAdapterInfo                  = WGPU_WAGYU_RESERVED_RANGE_BASE + 0x0001,
    WGPUSType_WagyuColorTargetState             = WGPU_WAGYU_RESERVED_RANGE_BASE + 0x0002,
    WGPUSType_WagyuCommandEncoderDescriptor     = WGPU_WAGYU_RESERVED_RANGE_BASE + 0x0003,
    WGPUSType_WagyuComputePipelineDescriptor    = WGPU_WAGYU_RESERVED_RANGE_BASE + 0x0004,
    WGPUSType_WagyuDeviceDescriptor             = WGPU_WAGYU_RESERVED_RANGE_BASE + 0x0005,
    WGPUSType_WagyuExternalTextureBindingEntry  = WGPU_WAGYU_RESERVED_RANGE_BASE + 0x0006,
    WGPUSType_WagyuExternalTextureBindingLayout = WGPU_WAGYU_RESERVED_RANGE_BASE + 0x0007,
    WGPUSType_WagyuFragmentState                = WGPU_WAGYU_RESERVED_RANGE_BASE + 0x0008,
    WGPUSType_WagyuInputTextureBindingLayout    = WGPU_WAGYU_RESERVED_RANGE_BASE + 0x0009,
    WGPUSType_WagyuRenderPassDescriptor         = WGPU_WAGYU_RESERVED_RANGE_BASE + 0x000A,
    WGPUSType_WagyuRenderPipelineDescriptor     = WGPU_WAGYU_RESERVED_RANGE_BASE + 0x000B,
    WGPUSType_WagyuShaderModuleDescriptor       = WGPU_WAGYU_RESERVED_RANGE_BASE + 0x000C,
    WGPUSType_WagyuSurfaceConfiguration         = WGPU_WAGYU_RESERVED_RANGE_BASE + 0x000D,
    WGPUSType_WagyuTextureDescriptor            = WGPU_WAGYU_RESERVED_RANGE_BASE + 0x000E,
    WGPUSType_WagyuForce32                      = 0x7FFFFFFF
} WGPUSType_Wagyu WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUWagyuDeviceFlushStatus
{
    WGPUWagyuDeviceFlushStatus_Success = 0x00000000,
    WGPUWagyuDeviceFlushStatus_Error   = 0x00000001,
    WGPUWagyuDeviceFlushStatus_Force32 = 0x7FFFFFFF
} WGPUWagyuDeviceFlushStatus WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUWagyuDevicePipelineBinaryCacheError
{
    WGPUWagyuDevicePipelineBinaryCacheError_Version = 0x00000000,
    WGPUWagyuDevicePipelineBinaryCacheError_Corrupt = 0x00000001,
    WGPUWagyuDevicePipelineBinaryCacheError_Link    = 0x00000002,
    WGPUWagyuDevicePipelineBinaryCacheError_Create  = 0x00000003,
    WGPUWagyuDevicePipelineBinaryCacheError_Force32 = 0x7FFFFFFF
} WGPUWagyuDevicePipelineBinaryCacheError WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUWagyuShaderLanguage
{
    WGPUWagyuShaderLanguage_Detect  = 0x00000000,
    WGPUWagyuShaderLanguage_GLSL    = 0x00000001,
    WGPUWagyuShaderLanguage_GLSLRAW = 0x00000002,
    WGPUWagyuShaderLanguage_WGSL    = 0x00000003,
    WGPUWagyuShaderLanguage_SPIRV   = 0x00000004,
    WGPUWagyuShaderLanguage_Force32 = 0x7FFFFFFF
} WGPUWagyuShaderLanguage WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUWagyuWGSLFeatureType
{
    WGPUWagyuWGSLFeatureType_Testing            = 0x00000001,
    WGPUWagyuWGSLFeatureType_UnsafeExperimental = 0x00000002,
    WGPUWagyuWGSLFeatureType_Experimental       = 0x00000004,
    WGPUWagyuWGSLFeatureType_All                = 0x00000007,
    WGPUWagyuWGSLFeatureType_Force32            = 0x7FFFFFFF
} WGPUWagyuWGSLFeatureType WGPU_ENUM_ATTRIBUTE;

typedef WGPUFlags WGPUWagyuFragmentStateFeaturesFlags;
static const WGPUWagyuFragmentStateFeaturesFlags WGPUWagyuFragmentStateFeaturesFlags_None                               = 0x00000000;
static const WGPUWagyuFragmentStateFeaturesFlags WGPUWagyuFragmentStateFeaturesFlags_RasterizationOrderAttachmentAccess = 0x00000001;

// These values extend the WGPUTextureUsage enum set from webgpu.h
static const WGPUTextureUsage WGPUTextureUsage_WagyuInputAttachment     = (WGPUTextureUsage)(0x0000000040000000);
static const WGPUTextureUsage WGPUTextureUsage_WagyuTransientAttachment = (WGPUTextureUsage)(0x0000000020000000);

// Forward declarations for callbacks
struct WGPUWagyuDevicePipelineBinaryCacheStatistics;
struct WGPUWagyuDevicePipelineBinaryEvent;
struct WGPUWagyuShaderEntryPointArray;

typedef void (*WGPUWagyuDeviceFlushCallback)(WGPUDevice device, WGPUWagyuDeviceFlushStatus status, void *userdata1, void *userdata2) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUWagyuExecuteCallback)(void *userdata1, void *userdata2) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUWagyuPipelineBinaryCacheStatisticsCallback)(WGPUDevice device, WGPUWagyuDeviceFlushStatus status, const struct WGPUWagyuDevicePipelineBinaryCacheStatistics *statistics, void *userdata1, void *userdata2) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUWagyuPipelineBinaryCallback)(WGPUDevice device, const struct WGPUWagyuDevicePipelineBinaryEvent *event, void *userdata1, void *userdata2) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUWagyuPipelineBinaryErrorCallback)(WGPUDevice device, WGPUStringView reason, WGPUStringView message, void *userdata1, void *userdata2) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUWagyuShaderModuleEntryPointsCallback)(WGPUShaderModule shaderModule, WGPUWagyuDeviceFlushStatus status, const struct WGPUWagyuShaderEntryPointArray *entryPoints, void *userdata1, void *userdata2) WGPU_FUNCTION_ATTRIBUTE;

typedef struct WGPUWagyuNrdpVersion
{
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    uint32_t rev;
} WGPUWagyuNrdpVersion WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_NRDP_VERSION_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuNrdpVersion, { /*.major = */ 0 _wgpu_COMMA /*.minor = */ 0 _wgpu_COMMA /*.patch = */ 0 _wgpu_COMMA /*.rev = */ 0 _wgpu_COMMA })

typedef struct WGPUWagyuAdapterInfo
{
    WGPUChainedStruct chain;
    uint32_t extensionLevel;
    WGPUWagyuNrdpVersion nrdpVersion;
} WGPUWagyuAdapterInfo WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_ADAPTER_INFO_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuAdapterInfo, { /*.chain*/ WGPU_WAGYU_CHAIN_INIT(WGPUSType_WagyuAdapterInfo) _wgpu_COMMA /*.level*/ 0 _wgpu_COMMA /*.nrdpVersion*/ WGPU_WAGYU_NRDP_VERSION_INIT _wgpu_COMMA })

typedef struct WGPUWagyuColorTargetState
{
    WGPUChainedStruct chain;
    WGPUOptionalBool usedAsInput;
} WGPUWagyuColorTargetState WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_COLOR_TARGET_STATE_INIT                 \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuColorTargetState, \
                                { /*.chain*/ WGPU_WAGYU_CHAIN_INIT(WGPUSType_WagyuColorTargetState) _wgpu_COMMA /*.usedAsInput*/ WGPUOptionalBool_Undefined _wgpu_COMMA })

typedef struct WGPUWagyuCommandEncoderDescriptor
{
    WGPUChainedStruct chain;
    WGPUOptionalBool measureExecutionTime;
} WGPUWagyuCommandEncoderDescriptor WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_COMMAND_ENCODER_DESCRIPTOR_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuCommandEncoderDescriptor, { /*.chain*/ WGPU_WAGYU_CHAIN_INIT(WGPUSType_WagyuCommandEncoderDescriptor) _wgpu_COMMA /*.measureExecutionTime*/ WGPUOptionalBool_Undefined _wgpu_COMMA })

typedef struct WGPUWagyuComputePipelineDescriptor
{
    WGPUChainedStruct chain;
    WGPUStringView cacheKey;
} WGPUWagyuComputePipelineDescriptor WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_COMPUTE_PIPELINE_DESCRIPTOR_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuComputePipelineDescriptor, { /*.chain*/ WGPU_WAGYU_CHAIN_INIT(WGPUSType_WagyuComputePipelineDescriptor) _wgpu_COMMA /*.cacheKey*/ WGPU_STRING_VIEW_INIT _wgpu_COMMA })

typedef struct WGPUWagyuOrigin2D
{
    uint32_t x;
    uint32_t y;
} WGPUWagyuOrigin2D WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_ORIGIN_2D_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuOrigin2D, { /* .x */ 0 _wgpu_COMMA /* .y */ 0 _wgpu_COMMA })

typedef struct WGPUWagyuCopyExternalImageSourceInfo
{
    WGPUStringView source;
    WGPUWagyuOrigin2D origin;
    WGPUBool flipY;
} WGPUWagyuCopyExternalImageSourceInfo WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_COPY_EXTERNAL_IMAGE_SOURCE_INFO_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuCopyExternalImageSourceInfo, { /* .source */ WGPU_STRING_VIEW_INIT _wgpu_COMMA /* .origin */ WGPU_WAGYU_ORIGIN_2D_INIT _wgpu_COMMA /* .flipY */ WGPU_FALSE _wgpu_COMMA })

typedef struct WGPUWagyuCopyExternalImageDestInfo
{
    WGPUTexture texture;
    uint32_t mipLevel;
    WGPUOrigin3D origin;
    WGPUTextureAspect aspect;
    WGPUPredefinedColorSpace colorSpace;
    WGPUBool premultipliedAlpha;
} WGPUWagyuCopyExternalImageDestInfo WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_COPY_EXTERNAL_IMAGE_DEST_INFO_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuCopyExternalImageDestInfo, { /* .texture */ NULL _wgpu_COMMA /* .mipLevel */ 0 _wgpu_COMMA /* .origin */ WGPU_ORIGIN_3D_INIT _wgpu_COMMA /* .aspect */ WGPUTextureAspect_All _wgpu_COMMA /* .colorSpace */ WGPUPredefinedColorSpace_SRGB _wgpu_COMMA /* .premultipliedAlpha */ WGPU_FALSE _wgpu_COMMA })

typedef struct WGPUWagyuDeviceDescriptor
{
    WGPUChainedStruct chain;
    WGPUOptionalBool dataBufferNeedsDetach;
    WGPUOptionalBool wantsIndirectRendering;
    WGPUOptionalBool wantsBufferClear;
    WGPUOptionalBool wantsTextureClear;
} WGPUWagyuDeviceDescriptor WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_DEVICE_DESCRIPTOR_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuDeviceDescriptor, { /*.chain*/ WGPU_WAGYU_CHAIN_INIT(WGPUSType_WagyuDeviceDescriptor) _wgpu_COMMA /*.dataBufferNeedsDetach*/ WGPUOptionalBool_Undefined _wgpu_COMMA /*.wantsIndirectRendering*/ WGPUOptionalBool_Undefined _wgpu_COMMA /*.wantsBufferClear*/ WGPUOptionalBool_Undefined _wgpu_COMMA /*.wantsTextureClear*/ WGPUOptionalBool_Undefined _wgpu_COMMA })

typedef struct WGPUWagyuDeviceFlushCallbackInfo
{
    WGPUChainedStruct *nextInChain;
    WGPUCallbackMode mode;
    WGPUWagyuDeviceFlushCallback callback;
    void *userdata1;
    void *userdata2;
} WGPUWagyuDeviceFlushCallbackInfo WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_DEVICE_FLUSH_CALLBACK_INFO_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuDeviceFlushCallbackInfo, { /*.nextInChain = */ NULL _wgpu_COMMA /*.mode = */ WGPUCallbackMode_AllowSpontaneous _wgpu_COMMA /*.callback = */ NULL _wgpu_COMMA /*.userdata1 = */ NULL _wgpu_COMMA /*.userdata2 = */ NULL _wgpu_COMMA })

typedef struct WGPUWagyuExecuteCallbackInfo
{
    WGPUChainedStruct *nextInChain;
    WGPUCallbackMode mode;
    WGPUWagyuExecuteCallback callback;
    void *userdata1;
    void *userdata2;
} WGPUWagyuExecuteCallbackInfo WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_EXECUTE_CALLBACK_INFO_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuExecuteCallbackInfo, { /*.nextInChain = */ NULL _wgpu_COMMA /*.mode = */ WGPUCallbackMode_AllowSpontaneous _wgpu_COMMA /*.callback = */ NULL _wgpu_COMMA /*.userdata1 = */ NULL _wgpu_COMMA /*.userdata2 = */ NULL _wgpu_COMMA })

typedef struct WGPUWagyuDevicePipelineBinary
{
    size_t binarySize;
    void *binary;
    size_t blobKeySize;
    void *blobKey;
} WGPUWagyuDevicePipelineBinary WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_DEVICE_PIPELINE_BINARY_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuDevicePipelineBinary, { /*.binarySize*/ 0 _wgpu_COMMA /*.binary*/ NULL _wgpu_COMMA /*.blobKeySize*/ 0 _wgpu_COMMA /*.blobKey*/ NULL _wgpu_COMMA })

typedef struct WGPUWagyuDevicePipelineBinaryBlobKey
{
    size_t size;
    const void *data;
} WGPUWagyuDevicePipelineBinaryBlobKey WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_DEVICE_PIPELINE_BINARY_BLOB_KEY_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuDevicePipelineBinaryBlobKey, { /*.size*/ 0 _wgpu_COMMA /*.data*/ NULL _wgpu_COMMA })

typedef struct WGPUWagyuDevicePipelineBinaryCacheKey
{
    WGPUStringView cacheKey;
    size_t blobKeysLength;
    WGPUWagyuDevicePipelineBinaryBlobKey *blobKeys;
} WGPUWagyuDevicePipelineBinaryCacheKey WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_DEVICE_PIPELINE_BINARY_CACHE_KEY_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuDevicePipelineBinaryCacheKey, { /*.cacheKey*/ WGPU_STRING_VIEW_INIT _wgpu_COMMA /*.blobKeysLength*/ 0 _wgpu_COMMA /*.blobKeys*/ NULL _wgpu_COMMA })

typedef struct WGPUWagyuDevicePipelineBinaryCacheStatistics
{
    uint32_t hits;
    uint32_t misses;
    size_t entryCount;
    WGPUStringView *entries;
    size_t errorCount;
    WGPUWagyuDevicePipelineBinaryCacheError *errors;
} WGPUWagyuDevicePipelineBinaryCacheStatistics WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_DEVICE_PIPELINE_BINARY_CACHE_STATISTICS_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuDevicePipelineBinaryCacheStatistics, { /*.hits*/ 0 _wgpu_COMMA /*.misses*/ 0 _wgpu_COMMA /*.entryCount*/ 0 _wgpu_COMMA /*.entries*/ NULL _wgpu_COMMA /*.errorCount*/ 0 _wgpu_COMMA /*.errors*/ NULL _wgpu_COMMA })

typedef struct WGPUWagyuPipelineBinaryCacheStatisticsCallbackInfo
{
    WGPUChainedStruct *nextInChain;
    WGPUCallbackMode mode;
    WGPUWagyuPipelineBinaryCacheStatisticsCallback callback;
    void *userdata1;
    void *userdata2;
} WGPUWagyuPipelineBinaryCacheStatisticsCallbackInfo WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_PIPELINE_BINARY_CACHE_STATISTICS_CALLBACK_INFO_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuPipelineBinaryCacheStatisticsCallbackInfo, { /*.nextInChain = */ NULL _wgpu_COMMA /*.mode = */ WGPUCallbackMode_AllowSpontaneous _wgpu_COMMA /*.callback = */ NULL _wgpu_COMMA /*.userdata1 = */ NULL _wgpu_COMMA /*.userdata2 = */ NULL _wgpu_COMMA })

typedef struct WGPUWagyuDevicePipelineBinaryData
{
    size_t binariesLength;
    WGPUWagyuDevicePipelineBinary *binaries;
    size_t cacheKeysLength;
    WGPUWagyuDevicePipelineBinaryCacheKey *cacheKeys;
} WGPUWagyuDevicePipelineBinaryData WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_DEVICE_PIPELINE_BINARY_DATA_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuDevicePipelineBinaryData, { /*.binariesLength*/ 0 _wgpu_COMMA /*.binaries*/ NULL _wgpu_COMMA /*.cacheKeysLength*/ 0 _wgpu_COMMA /*.cacheKeys*/ NULL _wgpu_COMMA })

typedef struct WGPUWagyuDevicePipelineBinaryEvent
{
    WGPUStringView cacheKey;
    WGPUStringView pipelineLabel;
    size_t binariesLength;
    const WGPUWagyuDevicePipelineBinary *binaries;
} WGPUWagyuDevicePipelineBinaryEvent WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_DEVICE_PIPELINE_BINARY_EVENT_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuDevicePipelineBinaryEvent, { /*.cacheKey*/ WGPU_STRING_VIEW_INIT _wgpu_COMMA /*.pipelineLabel*/ WGPU_STRING_VIEW_INIT _wgpu_COMMA /*.binariesLength*/ 0 _wgpu_COMMA /*.binaries*/ NULL _wgpu_COMMA })

typedef struct WGPUWagyuExternalTextureDescriptor
{
    const WGPUChainedStruct *nextInChain;
    WGPUStringView label;
    WGPUStringView source;
    WGPUPredefinedColorSpace colorSpace;
} WGPUWagyuExternalTextureDescriptor WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_EXTERNAL_TEXTURE_DESCRIPTOR_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuExternalTextureDescriptor, { /*nextInChain = */ NULL _wgpu_COMMA /*label = */ WGPU_STRING_VIEW_INIT _wgpu_COMMA /*source = */ WGPU_STRING_VIEW_INIT _wgpu_COMMA /*colorSpace = */ WGPUPredefinedColorSpace_SRGB _wgpu_COMMA })

typedef struct WGPUWagyuExternalTextureInfo
{
    uint32_t visibleWidth;
    uint32_t visibleHeight;
    uint32_t textureWidth;
    uint32_t textureHeight;
    int64_t pts;
} WGPUWagyuExternalTextureInfo WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_EXTERNAL_TEXTURE_INFO_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuExternalTextureInfo, { /*.visibleWidth*/ 0 _wgpu_COMMA /*.visibleHeight*/ 0 _wgpu_COMMA /*.textureWidth*/ 0 _wgpu_COMMA /*.textureHeight*/ 0 _wgpu_COMMA /*.pts*/ 0 _wgpu_COMMA })

typedef struct WGPUWagyuExternalTextureBindingEntry
{
    WGPUChainedStruct chain;
    WGPUWagyuExternalTexture externalTexture;
} WGPUWagyuExternalTextureBindingEntry WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_EXTERNAL_TEXTURE_BINDING_ENTRY_INIT                \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuExternalTextureBindingEntry, \
                                { /*.chain = */ WGPU_WAGYU_CHAIN_INIT(WGPUSType_WagyuExternalTextureBindingEntry) _wgpu_COMMA /*.externalTexture = */ NULL _wgpu_COMMA })

typedef struct WGPUWagyuExternalTextureBindingLayout
{
    WGPUChainedStruct chain;
} WGPUWagyuExternalTextureBindingLayout WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_EXTERNAL_TEXTURE_BINDING_LAYOUT_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuExternalTextureBindingLayout, { /*.chain*/ WGPU_WAGYU_CHAIN_INIT(WGPUSType_WagyuExternalTextureBindingLayout) _wgpu_COMMA })

typedef struct WGPUWagyuInputAttachmentState
{
    WGPUTextureFormat format;
    WGPUOptionalBool usedAsColor;
} WGPUWagyuInputAttachmentState WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_INPUT_ATTACHMENT_STATE_INIT                 \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuInputAttachmentState, \
                                { /*.format*/ WGPUTextureFormat_Undefined _wgpu_COMMA /*.usedAsColor*/ WGPUOptionalBool_Undefined _wgpu_COMMA })

typedef struct WGPUWagyuFragmentState
{
    WGPUChainedStruct chain;
    size_t inputCount;
    WGPU_NULLABLE WGPUWagyuInputAttachmentState *inputs;
    WGPUWagyuFragmentStateFeaturesFlags featureFlags;
} WGPUWagyuFragmentState WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_FRAGMENT_STATE_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuFragmentState, { /*.chain*/ WGPU_WAGYU_CHAIN_INIT(WGPUSType_WagyuFragmentState) _wgpu_COMMA /*.inputCount*/ 0 _wgpu_COMMA /*.inputs*/ NULL _wgpu_COMMA /*.featureFlags*/ WGPUWagyuFragmentStateFeaturesFlags_None _wgpu_COMMA })

typedef struct WGPUWagyuInputTextureBindingLayout
{
    WGPUChainedStruct chain;
    WGPUTextureViewDimension viewDimension;
} WGPUWagyuInputTextureBindingLayout WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_INPUT_TEXTURE_BINDING_LAYOUT_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuInputTextureBindingLayout, { /*.chain*/ WGPU_WAGYU_CHAIN_INIT(WGPUSType_WagyuInputTextureBindingLayout) _wgpu_COMMA /*.viewDimension*/ WGPUTextureViewDimension_2D _wgpu_COMMA })

typedef struct WGPUWagyuPipelineBinaryCallbackInfo
{
    WGPUChainedStruct *nextInChain;
    WGPUCallbackMode mode;
    WGPUWagyuPipelineBinaryCallback callback;
    void *userdata1;
    void *userdata2;
} WGPUWagyuPipelineBinaryCallbackInfo WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_PIPELINE_BINARY_CALLBACK_INFO_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuPipelineBinaryCallbackInfo, { /*.nextInChain = */ NULL _wgpu_COMMA /*.mode = */ WGPUCallbackMode_AllowSpontaneous _wgpu_COMMA /*.callback = */ NULL _wgpu_COMMA /*.userdata1 = */ NULL _wgpu_COMMA /*.userdata2 = */ NULL _wgpu_COMMA })

typedef struct WGPUWagyuPipelineBinaryErrorCallbackInfo
{
    WGPUChainedStruct *nextInChain;
    WGPUCallbackMode mode;
    WGPUWagyuPipelineBinaryErrorCallback callback;
    void *userdata1;
    void *userdata2;
} WGPUWagyuPipelineBinaryErrorCallbackInfo WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_PIPELINE_BINARY_ERROR_CALLBACK_INFO_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuPipelineBinaryErrorCallbackInfo, { /*.nextInChain = */ NULL _wgpu_COMMA /*.mode = */ WGPUCallbackMode_AllowSpontaneous _wgpu_COMMA /*.callback = */ NULL _wgpu_COMMA /*.userdata1 = */ NULL _wgpu_COMMA /*.userdata2 = */ NULL _wgpu_COMMA })

typedef struct WGPUWagyuRect
{
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
} WGPUWagyuRect WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_RECT_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuRect, { /* .x */ 0 _wgpu_COMMA /* .y */ 0 _wgpu_COMMA /* .width */ 0 _wgpu_COMMA /* .height */ 0 _wgpu_COMMA })

typedef struct WGPUWagyuRenderPassInputAttachment
{
    WGPUTextureView view;
    WGPU_NULLABLE WGPUColor *clearValue;
    WGPULoadOp loadOp;
    WGPUStoreOp storeOp;
} WGPUWagyuRenderPassInputAttachment WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_RENDER_PASS_INPUT_ATTACHMENT_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuRenderPassInputAttachment, { /* .view */ NULL _wgpu_COMMA /* .clearValue */ NULL _wgpu_COMMA /* .loadOp */ WGPULoadOp_Undefined _wgpu_COMMA /* .storeOp */ WGPUStoreOp_Undefined _wgpu_COMMA })

typedef struct WGPUWagyuRenderPassDescriptor
{
    WGPUChainedStruct chain;
    size_t inputAttachmentCount;
    WGPU_NULLABLE WGPUWagyuRenderPassInputAttachment *inputAttachments;
    WGPUOptionalBool pixelLocalStorageEnabled;
    uint32_t pixelLocalStorageSize;
} WGPUWagyuRenderPassDescriptor WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_RENDER_PASS_DESCRIPTOR_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuRenderPassDescriptor, { /* .chain */ WGPU_WAGYU_CHAIN_INIT(WGPUSType_WagyuRenderPassDescriptor) _wgpu_COMMA /* .inputAttachmentCount */ 0 _wgpu_COMMA /* .inputAttachments */ NULL _wgpu_COMMA /* .pixelLocalStorageEnabled */ WGPUOptionalBool_Undefined _wgpu_COMMA /* .pixelLocalStorageSize */ WGPU_WAGYU_PIXEL_LOCAL_STORAGE_SIZE_UNDEFINED _wgpu_COMMA })

typedef struct WGPUWagyuRenderPipelineDescriptor
{
    WGPUChainedStruct chain;
    WGPUStringView cacheKey;
} WGPUWagyuRenderPipelineDescriptor WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_RENDER_PIPELINE_DESCRIPTOR_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuRenderPipelineDescriptor, { /* .chain */ WGPU_WAGYU_CHAIN_INIT(WGPUSType_WagyuRenderPipelineDescriptor) _wgpu_COMMA /* .cacheKey */ WGPU_STRING_VIEW_INIT _wgpu_COMMA })

typedef struct WGPUWagyuShaderModuleEntryPointsCallbackInfo
{
    WGPUChainedStruct *nextInChain;
    WGPUCallbackMode mode;
    WGPUWagyuShaderModuleEntryPointsCallback callback;
    void *userdata1;
    void *userdata2;
} WGPUWagyuShaderModuleEntryPointsCallbackInfo WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_SHADER_MODULE_ENTRY_POINTS_CALLBACK_INFO_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuShaderModuleEntryPointsCallbackInfo, { /*.nextInChain = */ NULL _wgpu_COMMA /*.mode = */ WGPUCallbackMode_AllowSpontaneous _wgpu_COMMA /*.callback = */ NULL _wgpu_COMMA /*.userdata1 = */ NULL _wgpu_COMMA /*.userdata2 = */ NULL _wgpu_COMMA })

typedef struct WGPUWagyuShaderReflectionStructMember
{
    WGPUStringView name;
    uint32_t group;
    uint32_t binding;
    uint32_t offset;
    uint32_t size;
    uint32_t type;
    WGPUBool imageMultisampled;
    WGPUTextureViewDimension imageDimension;
    WGPUTextureFormat imageFormat;
} WGPUWagyuShaderReflectionStructMember WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_SHADER_REFLECTION_STRUCT_MEMBER_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuShaderReflectionStructMember, { /* .name */ WGPU_STRING_VIEW_INIT _wgpu_COMMA /* .group */ 0 _wgpu_COMMA /* .binding */ 0 _wgpu_COMMA /* .offset */ 0 _wgpu_COMMA /* .size */ 0 _wgpu_COMMA /* .type */ 0 _wgpu_COMMA /* .imageMultisampled */ WGPU_FALSE _wgpu_COMMA /* .imageDimension */ WGPUTextureViewDimension_Undefined _wgpu_COMMA /* .imageFormat */ WGPUTextureFormat_Undefined _wgpu_COMMA })

typedef struct WGPUWagyuShaderReflectionLocation
{
    WGPUStringView name;
    uint32_t location;
    uint32_t size;
    uint32_t type;
} WGPUWagyuShaderReflectionLocation WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_SHADER_REFLECTION_LOCATION_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuShaderReflectionLocation, { /* .name */ WGPU_STRING_VIEW_INIT _wgpu_COMMA /* .location */ 0 _wgpu_COMMA /* .size */ 0 _wgpu_COMMA /* .type */ 0 _wgpu_COMMA })

typedef struct WGPUWagyuShaderReflectionResource
{
    WGPUStringView name;
    uint32_t group;
    uint32_t binding;
    uint32_t bindingType;
    WGPUBool multisampled;
    WGPUTextureViewDimension dimension;
    WGPUTextureFormat format;
    uint64_t bufferSize;
} WGPUWagyuShaderReflectionResource WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_SHADER_REFLECTION_RESOURCE_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuShaderReflectionResource, { /* .name */ WGPU_STRING_VIEW_INIT _wgpu_COMMA /* .group */ 0 _wgpu_COMMA /* .binding */ 0 _wgpu_COMMA /* .bindingType */ 0 _wgpu_COMMA /* .multisampled */ WGPU_FALSE _wgpu_COMMA /* .dimension */ WGPUTextureViewDimension_Undefined _wgpu_COMMA /* .format */ WGPUTextureFormat_Undefined _wgpu_COMMA /* .bufferSize */ 0 _wgpu_COMMA })

typedef struct WGPUWagyuShaderReflectionSpecializationConstant
{
    uint32_t id;
    uint32_t internalId;
    uint32_t type;
    WGPUStringView name;
} WGPUWagyuShaderReflectionSpecializationConstant WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_SHADER_REFLECTION_SPECIALIZATION_CONSTANT_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuShaderReflectionSpecializationConstant, { /* .id */ 0 _wgpu_COMMA /* .internalId */ 0 _wgpu_COMMA /* .type */ 0 _wgpu_COMMA /* .name */ WGPU_STRING_VIEW_INIT _wgpu_COMMA })

typedef struct WGPUWagyuShaderReflectionData
{
    size_t resourceCount;
    WGPUWagyuShaderReflectionResource *resources;
    size_t constantCount;
    WGPUWagyuShaderReflectionSpecializationConstant *constants;
    size_t uniformCount;
    WGPUWagyuShaderReflectionStructMember *uniforms;
    size_t attributeCount;
    WGPUWagyuShaderReflectionLocation *attributes;
    WGPUStringView wgsl;
} WGPUWagyuShaderReflectionData WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_SHADER_REFLECTION_DATA_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuShaderReflectionData, { /* .resourceCount */ 0 _wgpu_COMMA /* .resources */ NULL _wgpu_COMMA /* .constantCount */ 0 _wgpu_COMMA /* .constants */ NULL _wgpu_COMMA /* .uniformCount */ 0 _wgpu_COMMA /* .uniforms */ NULL _wgpu_COMMA /* .attributeCount */ 0 _wgpu_COMMA /* .attributes */ NULL _wgpu_COMMA /* .wgsl */ WGPU_STRING_VIEW_INIT _wgpu_COMMA })

typedef struct WGPUWagyuShaderEntryPoint
{
    WGPUStringView entryPoint;
    WGPUShaderStage stage;
    WGPUWagyuShaderReflectionData reflection;
} WGPUWagyuShaderEntryPoint WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_SHADER_ENTRY_POINT_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuShaderEntryPoint, { /* .entryPoint */ WGPU_STRING_VIEW_INIT _wgpu_COMMA /* .stage */ WGPUShaderStage_NONE _wgpu_COMMA /* .reflection */ WGPU_WAGYU_SHADER_REFLECTION_DATA_INIT _wgpu_COMMA })

typedef struct WGPUWagyuShaderEntryPointArray
{
    size_t entryPointCount;
    WGPUWagyuShaderEntryPoint *entryPoints;
} WGPUWagyuShaderEntryPointArray WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_SHADER_ENTRY_POINT_ARRAY_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuShaderEntryPointArray, { /* .entryPointCount */ 0 _wgpu_COMMA /* .entryPoints */ NULL _wgpu_COMMA })

typedef struct WGPUWagyuShaderModuleCompilationHint
{
    WGPUChainedStruct *nextInChain;
    WGPUStringView entryPoint;
    /**
     * If set to NULL, it will be treated as "auto"
     */
    WGPUPipelineLayout layout;
} WGPUWagyuShaderModuleCompilationHint;

#define WGPU_WAGYU_SHADER_MODULE_COMPILATION_HINT_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuShaderModuleCompilationHint, { /* .nextInChain */ NULL _wgpu_COMMA /* .entryPoint */ WGPU_STRING_VIEW_INIT _wgpu_COMMA /* .layout */ NULL _wgpu_COMMA })

typedef struct WGPUWagyuShaderModuleDescriptor
{
    WGPUChainedStruct chain;
    size_t codeSize; // bytes
    const void *code;
    WGPUWagyuShaderLanguage language;
    size_t compilationHintCount;
    const struct WGPUWagyuShaderModuleCompilationHint *compilationHints;
} WGPUWagyuShaderModuleDescriptor WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_SHADER_MODULE_DESCRIPTOR_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuShaderModuleDescriptor, { /*.chain=*/WGPU_WAGYU_CHAIN_INIT(WGPUSType_WagyuShaderModuleDescriptor) _wgpu_COMMA /*.codeSize*/ 0 _wgpu_COMMA /*.code*/ NULL _wgpu_COMMA /*.language*/ WGPUWagyuShaderLanguage_Detect _wgpu_COMMA /*.compilationHintCount*/ 0 _wgpu_COMMA /*.compilationHints*/ NULL _wgpu_COMMA })

typedef struct WGPUWagyuStringArray
{
    size_t stringCount;
    WGPU_NULLABLE WGPUStringView *strings;
} WGPUWagyuStringArray WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_STRING_ARRAY_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuStringArray, { /* .stringCount */ 0 _wgpu_COMMA /* .strings */ NULL _wgpu_COMMA })

typedef struct WGPUWagyuSurfaceConfiguration
{
    WGPUChainedStruct chain;
    int32_t *indirectRenderTargets;
    WGPUPredefinedColorSpace colorSpace;
    WGPUToneMappingMode toneMappingMode;
} WGPUWagyuSurfaceConfiguration WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_SURFACE_CONFIGURATION_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuSurfaceConfiguration, { /*.chain=*/WGPU_WAGYU_CHAIN_INIT(WGPUSType_WagyuSurfaceConfiguration) _wgpu_COMMA /*.indirectRenderTargets*/ NULL _wgpu_COMMA /*.colorSpace*/ WGPUPredefinedColorSpace_SRGB _wgpu_COMMA /*.toneMappingMode*/ WGPUToneMappingMode_Standard _wgpu_COMMA })

typedef struct WGPUWagyuTextureDescriptor
{
    WGPUChainedStruct chain;
    WGPUBool useSurfaceCache;
} WGPUWagyuTextureDescriptor WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_TEXTURE_DESCRIPTOR_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuTextureDescriptor, { /*.chain = */ WGPU_WAGYU_CHAIN_INIT(WGPUSType_WagyuTextureDescriptor) _wgpu_COMMA /*.useSurfaceCache = */ WGPU_TRUE _wgpu_COMMA })

typedef struct WGPUWagyuWGSLFeatureTypeArray
{
    size_t featureCount;
    WGPU_NULLABLE WGPUWagyuWGSLFeatureType *features;
} WGPUWagyuWGSLFeatureTypeArray WGPU_STRUCTURE_ATTRIBUTE;

#define WGPU_WAGYU_WGSL_FEATURE_TYPE_ARRAY_INIT \
    WGPU_WAGYU_MAKE_INIT_STRUCT(WGPUWagyuWGSLFeatureTypeArray, { /* .featureCount */ 0 _wgpu_COMMA /* .features */ NULL _wgpu_COMMA })

#if defined(__cplusplus) && !defined(__cppcheck)
extern "C" {
#endif

WGPU_EXPORT WGPUBackendType wgpuWagyuAdapterGetBackend(WGPUAdapter adapter) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuAdapterGetExtensions(WGPUAdapter adapter, WGPUWagyuStringArray *extensions) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuAdapterGetGraphicsReport(WGPUAdapter adapter, WGPUStringView *graphicsReport) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuAdapterGetName(WGPUAdapter adapter, WGPUStringView *name) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT WGPUDevice wgpuWagyuAdapterRequestDeviceSync(WGPUAdapter adapter, WGPU_NULLABLE const WGPUDeviceDescriptor *options) WGPU_FUNCTION_ATTRIBUTE;

WGPU_EXPORT void wgpuWagyuCommandEncoderBlit(WGPUCommandEncoder commandEncoder, const WGPUTexelCopyTextureInfo *source, const WGPUExtent3D *sourceExtent, const WGPUTexelCopyTextureInfo *destination, const WGPUExtent3D *destinationExtent, WGPUFilterMode filter) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuCommandEncoderExecuteCallback(WGPUCommandEncoder commandEncoder, WGPUWagyuExecuteCallbackInfo callbackInfo) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuCommandEncoderGenerateMipmap(WGPUCommandEncoder commandEncoder, WGPUTexture texture) WGPU_FUNCTION_ATTRIBUTE;

WGPU_EXPORT void wgpuWagyuComputePassEncoderExecuteCallback(WGPUComputePassEncoder computePassEncoder, WGPUWagyuExecuteCallbackInfo callbackInfo) WGPU_FUNCTION_ATTRIBUTE;

WGPU_EXPORT void wgpuWagyuDeviceClearPipelineBinaryCache(WGPUDevice device) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuDeviceEnableImaginationWorkarounds(WGPUDevice device, WGPUBool enable) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuDeviceGetExtensions(WGPUDevice device, WGPUWagyuStringArray *extensions) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT WGPUFuture wgpuWagyuDeviceFlush(WGPUDevice device, WGPUWagyuDeviceFlushCallbackInfo callbackInfo) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT WGPUWagyuExternalTexture wgpuWagyuDeviceImportExternalTexture(WGPUDevice device, const WGPUWagyuExternalTextureDescriptor *descriptor) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuDeviceIntrospectShaderCode(WGPUDevice device, WGPUShaderStage stages, const WGPUShaderModuleDescriptor *descriptor, WGPUWagyuShaderEntryPointArray *entryPoints) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT WGPUFuture wgpuWagyuDevicePipelineBinaryCacheStatistics(WGPUDevice device, WGPUWagyuPipelineBinaryCacheStatisticsCallbackInfo callbackInfo) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuDevicePopulatePipelineBinaryCache(WGPUDevice device, const WGPUWagyuDevicePipelineBinaryData *data) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuDeviceSetPipelineBinaryCallback(WGPUDevice device, WGPUWagyuPipelineBinaryCallbackInfo callbackInfo) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuDeviceSetPipelineBinaryErrorCallback(WGPUDevice device, WGPUWagyuPipelineBinaryErrorCallbackInfo callbackInfo) WGPU_FUNCTION_ATTRIBUTE;

WGPU_EXPORT void wgpuWagyuExternalTextureAddRef(WGPUWagyuExternalTexture externalTexture) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuExternalTextureGetInfo(WGPUWagyuExternalTexture externalTexture, WGPUWagyuExternalTextureInfo *info) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuExternalTextureRelease(WGPUWagyuExternalTexture externalTexture) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuExternalTextureSetLabel(WGPUWagyuExternalTexture externalTexture, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;

WGPU_EXPORT void wgpuWagyuInstanceEnableImaginationWorkarounds(WGPUInstance instance, WGPUBool enable) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT uint32_t wgpuWagyuInstanceGetApiVersion(WGPUInstance instance) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT WGPUBackendType wgpuWagyuInstanceGetBackend(WGPUInstance instance) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuInstanceGetExposedWgslFeatures(WGPUInstance instance, WGPUWagyuWGSLFeatureTypeArray *wgslFeatures) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT WGPUTextureFormat wgpuWagyuInstanceGetScreenDirectFormat(WGPUInstance instance) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT WGPUTextureFormat wgpuWagyuInstanceGetScreenIndirectFormat(WGPUInstance instance) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT WGPUBool wgpuWagyuInstanceGetSync(WGPUInstance instance) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT WGPUAdapter wgpuWagyuInstanceRequestAdapterSync(WGPUInstance instance, WGPU_NULLABLE const WGPURequestAdapterOptions *options) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuInstanceSetCommandBufferLimit(WGPUInstance instance, uint32_t limit) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuInstanceSetExposedWgslFeatures(WGPUInstance instance, const WGPUWagyuWGSLFeatureTypeArray *wgslFeatures) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuInstanceSetImmediate(WGPUInstance instance, WGPUBool enabled) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuInstanceSetRunBarriersOnIncoherent(WGPUInstance instance, WGPUBool run) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuInstanceSetStagingBufferCacheSize(WGPUInstance instance, uint32_t size) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuInstanceSetSync(WGPUInstance instance, WGPUBool sync) WGPU_FUNCTION_ATTRIBUTE;

WGPU_EXPORT void wgpuWagyuQueueCopyExternalImageToTexture(WGPUQueue queue, const WGPUWagyuCopyExternalImageSourceInfo *source, const WGPUWagyuCopyExternalImageDestInfo *destination, const WGPUExtent3D *copySize) WGPU_FUNCTION_ATTRIBUTE;

WGPU_EXPORT void wgpuWagyuRenderBundleEncoderClearColorAttachments(WGPURenderBundleEncoder renderBundleEncoder, const WGPUWagyuRect *rect, uint32_t baseAttachment, uint32_t numAttachments, const WGPUColor *color, uint32_t baseArrayLayer, uint32_t layerCount) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuRenderBundleEncoderClearDepthAttachment(WGPURenderBundleEncoder renderBundleEncoder, const WGPUWagyuRect *rect, float depth, uint32_t baseArrayLayer, uint32_t layerCount) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuRenderBundleEncoderClearPixelLocalStorage(WGPURenderBundleEncoder renderBundleEncoder, uint32_t offset, uint32_t size, WGPU_NULLABLE const uint32_t *values) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuRenderBundleEncoderClearStencilAttachment(WGPURenderBundleEncoder renderBundleEncoder, const WGPUWagyuRect *rect, uint32_t stencil, uint32_t baseArrayLayer, uint32_t layerCount) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuRenderBundleEncoderExecuteCallback(WGPURenderBundleEncoder renderBundleEncoder, WGPUWagyuExecuteCallbackInfo callbackInfo) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuRenderBundleEncoderSetScissorRect(WGPURenderBundleEncoder renderBundleEncoder, uint32_t x, uint32_t y, uint32_t width, uint32_t height) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuRenderBundleEncoderSetScissorRectIndirect(WGPURenderBundleEncoder renderBundleEncoder, uint64_t indirectOffset, const uint32_t *indirectBuffer, size_t indirectBufferCount) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuRenderBundleEncoderSetViewport(WGPURenderBundleEncoder renderBundleEncoder, float x, float y, float width, float height, float minDepth, float maxDepth) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuRenderBundleEncoderSetViewportWithDepthIndirect(WGPURenderBundleEncoder renderBundleEncoder, uint64_t indirectOffset, const float *indirectBuffer, size_t indirectBufferCount) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuRenderBundleEncoderSetViewportWithoutDepthIndirect(WGPURenderBundleEncoder renderBundleEncoder, uint64_t indirectOffset, const float *indirectBuffer, size_t indirectBufferCount) WGPU_FUNCTION_ATTRIBUTE;

WGPU_EXPORT void wgpuWagyuRenderPassEncoderClearColorAttachments(WGPURenderPassEncoder renderPassEncoder, const WGPUWagyuRect *rect, uint32_t baseAttachment, uint32_t numAttachments, const WGPUColor *color, uint32_t baseArrayLayer, uint32_t layerCount) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuRenderPassEncoderClearDepthAttachment(WGPURenderPassEncoder renderPassEncoder, const WGPUWagyuRect *rect, float depth, uint32_t baseArrayLayer, uint32_t layerCount) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuRenderPassEncoderClearStencilAttachment(WGPURenderPassEncoder renderPassEncoder, const WGPUWagyuRect *rect, uint32_t stencil, uint32_t baseArrayLayer, uint32_t layerCount) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuRenderPassEncoderClearPixelLocalStorage(WGPURenderPassEncoder renderPassEncoder, uint32_t offset, uint32_t size, WGPU_NULLABLE const uint32_t *values) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuRenderPassEncoderExecuteBundle(WGPURenderPassEncoder renderPassEncoder, WGPURenderBundle bundle) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuRenderPassEncoderExecuteCallback(WGPURenderPassEncoder renderPassEncoder, WGPUWagyuExecuteCallbackInfo callbackInfo) WGPU_FUNCTION_ATTRIBUTE;

WGPU_EXPORT void wgpuWagyuShaderEntryPointArrayFreeMembers(WGPUWagyuShaderEntryPointArray shaderEntryPointArray) WGPU_FUNCTION_ATTRIBUTE;

WGPU_EXPORT void wgpuWagyuShaderModuleDestroy(WGPUShaderModule shaderModule) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT WGPUFuture wgpuWagyuShaderModuleEntryPoints(WGPUShaderModule shaderModule, uint32_t stage, WGPUWagyuShaderModuleEntryPointsCallbackInfo callbackInfo) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuShaderModuleIntrospect(WGPUShaderModule shaderModule, WGPUShaderStage stages, WGPUWagyuShaderEntryPointArray *shaderEntryPointArray) WGPU_FUNCTION_ATTRIBUTE;

WGPU_EXPORT void wgpuWagyuStringArrayFreeMembers(WGPUWagyuStringArray stringArray) WGPU_FUNCTION_ATTRIBUTE;

WGPU_EXPORT void wgpuWagyuSurfaceDestroy(WGPUSurface surface) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT WGPUTexture wgpuWagyuSurfaceGetCurrentDepthStencilTexture(WGPUSurface surface) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT float wgpuWagyuSurfaceGetHeight(WGPUSurface surface) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT float wgpuWagyuSurfaceGetWidth(WGPUSurface surface) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT float wgpuWagyuSurfaceGetX(WGPUSurface surface) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT float wgpuWagyuSurfaceGetY(WGPUSurface surface) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuSurfacePresent(WGPUSurface surface, WGPUTexture target) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuSurfaceSetHeight(WGPUSurface surface, float height) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuSurfaceSetWidth(WGPUSurface surface, float width) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuSurfaceSetX(WGPUSurface surface, float x) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuSurfaceSetY(WGPUSurface surface, float y) WGPU_FUNCTION_ATTRIBUTE;

WGPU_EXPORT WGPUBool wgpuWagyuTextureIsSwapchain(WGPUTexture texture) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuWagyuTextureReadPixels(WGPUTexture texture, void *data, size_t dataSize) WGPU_FUNCTION_ATTRIBUTE;

WGPU_EXPORT void wgpuWagyuWGSLFeatureTypeArrayFreeMembers(WGPUWagyuWGSLFeatureTypeArray wgslFeatureTypeArray) WGPU_FUNCTION_ATTRIBUTE;

#if defined(__cplusplus) && !defined(__cppcheck)
} // extern "C"
#endif

#endif /* WEBGPU_WAGYU_H */
