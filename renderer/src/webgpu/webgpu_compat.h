#ifndef WEBGPU_COMPAT_H
#define WEBGPU_COMPAT_H

#define WGPU_DEPTH_CLEAR_VALUE_UNDEFINED NAN

#define WGPUTexelCopyBufferInfo WGPUImageCopyBuffer
#define WGPUTexelCopyTextureInfo WGPUImageCopyTexture
#define WGPUTexelCopyBufferLayout WGPUTextureDataLayout
#define WGPUInstanceCapabilities WGPUInstanceFeatures

#if !defined(WGPUOptionalBool)
#define WGPUOptionalBool int32_t
#define WGPUOptionalBool_False 0x00000000
#define WGPUOptionalBool_True 0x00000001
#define WGPUOptionalBool_Undefined 0x00000002
#define WGPUOptionalBool_Force32 0x7FFFFFFF
#endif

#define WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal                      \
    WGPUSurfaceGetCurrentTextureStatus_Success

#define WGPUMapAsyncStatus_Success WGPUBufferMapAsyncStatus_Success
#define WGPUMapAsyncStatus_Force32 WGPUBufferMapAsyncStatus_Force32

#define WGPUSType_ShaderSourceWGSL WGPUSType_ShaderModuleWGSLDescriptor
#define WGPUStatus int

#define WGPUStatus_Success 1

#define wgpuGetInstanceCapabilities wgpuGetInstanceFeatures

#define WGPUMapAsyncStatus WGPUBufferMapAsyncStatus
#define WGPUShaderSourceWGSL WGPUShaderModuleWGSLDescriptor
#define WGPURenderPassMaxDrawCount WGPURenderPassDescriptorMaxDrawCount
#define WGPUWGSLLanguageFeatureName WGPUWGSLFeatureName

#define WGPUEmscriptenSurfaceSourceCanvasHTMLSelector                          \
    WGPUSurfaceDescriptorFromCanvasHTMLSelector
#define WGPUWGSLLanguageFeatureName_Force32 WGPUWGSLFeatureName_Force32
#define WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector                    \
    WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector
#define WGPUSType_RenderPassMaxDrawCount                                       \
    WGPUSType_RenderPassDescriptorMaxDrawCount
#define WGPUSType_ShaderSourceWGSL WGPUSType_ShaderModuleWGSLDescriptor
#define WGPUWGSLLanguageFeatureName_Packed4x8IntegerDotProduct                 \
    WGPUWGSLFeatureName_Packed4x8IntegerDotProduct
#define WGPUWGSLLanguageFeatureName_PointerCompositeAccess                     \
    WGPUWGSLFeatureName_PointerCompositeAccess
#define WGPUWGSLLanguageFeatureName_ReadonlyAndReadwriteStorageTextures        \
    WGPUWGSLFeatureName_ReadonlyAndReadwriteStorageTextures
// #define WGPUWGSLLanguageFeatureName_Undefined WGPUWGSLFeatureName_Undefined
#define WGPUWGSLLanguageFeatureName_UnrestrictedPointerParameters              \
    WGPUWGSLFeatureName_UnrestrictedPointerParameters

#define WGPU_STRING_VIEW(s) s
#define WGPU_STRING_VIEW_INIT NULL
#define WGPU_STRING_VIEW_TO_STRING(s) std::string(s)
#define WGPU_STRING_VIEW_TO_CSTR(s) s
#define WGPU_STRING_VIEW_FROM_STRING(s) strdup(s.c_str())
#define WGPU_STRING_VIEW_FREE(s) free(const_cast<char*>(s))
#define WGPUStringView const char*

#define WGPU_ADDREF(d, o) wgpu##d##Reference(o)

#define WGPU_CHECK_STATUS(s) s

#if defined(__cplusplus)
#if __cplusplus >= 201103L
#define WGPU_MAKE_INIT_STRUCT(type, value) (type value)
#else
#define WGPU_MAKE_INIT_STRUCT(type, value) value
#endif
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define WGPU_MAKE_INIT_STRUCT(type, value) ((type)value)
#else
#define WGPU_MAKE_INIT_STRUCT(type, value) value
#endif

#ifndef _wgpu_COMMA
#define _wgpu_COMMA ,
#endif

#define WGPU_ADAPTER_INFO_INIT                                                 \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUAdapterInfo,                                                       \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.vendor=*/NULL                    \
             _wgpu_COMMA /*.architecture=*/NULL _wgpu_COMMA /*.device=*/NULL   \
                 _wgpu_COMMA /*.description=*/NULL                             \
                     _wgpu_COMMA                        /*.backendType=*/      \
         {} _wgpu_COMMA /*.adapterType=*/{} _wgpu_COMMA /*.vendorID=*/         \
         {} _wgpu_COMMA /*.deviceID=*/{} _wgpu_COMMA})

#define WGPU_BIND_GROUP_DESCRIPTOR_INIT                                        \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUBindGroupDescriptor,                                               \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.label=*/NULL                     \
             _wgpu_COMMA /*.layout=*/{} _wgpu_COMMA /*.entryCount=*/           \
         {} _wgpu_COMMA /*.entries=*/{} _wgpu_COMMA})

#define WGPU_BIND_GROUP_ENTRY_INIT                                             \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUBindGroupEntry,                                                    \
        {/*.nextInChain=*/NULL                                                 \
             _wgpu_COMMA /*.binding=*/{} _wgpu_COMMA /*.buffer=*/NULL          \
                 _wgpu_COMMA /*.offset=*/0 _wgpu_COMMA /*.size=*/              \
                     WGPU_WHOLE_SIZE _wgpu_COMMA /*.sampler=*/NULL             \
                         _wgpu_COMMA /*.textureView=*/NULL _wgpu_COMMA})

#define WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT                                 \
    WGPU_MAKE_INIT_STRUCT(WGPUBindGroupLayoutDescriptor,                       \
                          {/*.nextInChain=*/NULL _wgpu_COMMA /*.label=*/NULL   \
                               _wgpu_COMMA /*.entryCount=*/                    \
                           {} _wgpu_COMMA /*.entries=*/{} _wgpu_COMMA})

#define WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT                                      \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUBindGroupLayoutEntry,                                              \
        {/*.nextInChain=*/NULL _wgpu_COMMA             /*.binding=*/           \
         {} _wgpu_COMMA /*.visibility=*/{} _wgpu_COMMA /*.buffer=*/            \
             WGPU_BUFFER_BINDING_LAYOUT_INIT                                   \
                 _wgpu_COMMA /*.sampler=*/WGPU_SAMPLER_BINDING_LAYOUT_INIT     \
                     _wgpu_COMMA /*.texture=*/WGPU_TEXTURE_BINDING_LAYOUT_INIT \
                         _wgpu_COMMA /*.storageTexture=*/                      \
                             WGPU_STORAGE_TEXTURE_BINDING_LAYOUT_INIT          \
                                 _wgpu_COMMA})

#define WGPU_BLEND_COMPONENT_INIT                                              \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUBlendComponent,                                                    \
        {/*.operation=*/WGPUBlendOperation_Add _wgpu_COMMA /*.srcFactor=*/     \
             WGPUBlendFactor_One                                               \
                 _wgpu_COMMA /*.dstFactor=*/WGPUBlendFactor_Zero _wgpu_COMMA})

#define WGPU_BLEND_STATE_INIT                                                  \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUBlendState,                                                        \
        {/*.color=*/WGPU_BLEND_COMPONENT_INIT _wgpu_COMMA /*.alpha=*/          \
             WGPU_BLEND_COMPONENT_INIT _wgpu_COMMA})

#define WGPU_BUFFER_BINDING_LAYOUT_INIT                                        \
    WGPU_MAKE_INIT_STRUCT(WGPUBufferBindingLayout,                             \
                          {/*.nextInChain=*/NULL _wgpu_COMMA /*.type=*/        \
                               WGPUBufferBindingType_Undefined                 \
                                   _wgpu_COMMA /*.hasDynamicOffset=*/          \
                           0 _wgpu_COMMA /*.minBindingSize=*/0 _wgpu_COMMA})

#define WGPU_BUFFER_DESCRIPTOR_INIT                                            \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUBufferDescriptor,                                                  \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.label=*/NULL                     \
             _wgpu_COMMA /*.usage=*/{} _wgpu_COMMA /*.size=*/                  \
         {} _wgpu_COMMA /*.mappedAtCreation=*/0 _wgpu_COMMA})

#define WGPU_BUFFER_MAP_CALLBACK_INFO_INIT                                     \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUBufferMapCallbackInfo,                                             \
        {/*.nextInChain=*/NULL                                                 \
             _wgpu_COMMA /*.mode=*/{} _wgpu_COMMA /*.callback=*/NULL           \
                 _wgpu_COMMA /*.userdata1=*/NULL                               \
                     _wgpu_COMMA /*.userdata2=*/NULL _wgpu_COMMA})

#define WGPU_COLOR_INIT                                                        \
    WGPU_MAKE_INIT_STRUCT(WGPUColor,                                           \
                          {/*.r=*/{} _wgpu_COMMA /*.g=*/{} _wgpu_COMMA /*.b=*/ \
                           {} _wgpu_COMMA /*.a=*/{} _wgpu_COMMA})

#define WGPU_COLOR_TARGET_STATE_INIT                                           \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUColorTargetState,                                                  \
        {/*.nextInChain=*/NULL                                                 \
             _wgpu_COMMA /*.format=*/{} _wgpu_COMMA /*.blend=*/NULL            \
                 _wgpu_COMMA /*.writeMask=*/WGPUColorWriteMask_All             \
                     _wgpu_COMMA})

#define WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT                                    \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUCommandBufferDescriptor,                                           \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.label=*/NULL _wgpu_COMMA})

#define WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT                                   \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUCommandEncoderDescriptor,                                          \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.label=*/NULL _wgpu_COMMA})

#define WGPU_COMPILATION_INFO_INIT                                             \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUCompilationInfo,                                                   \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.messageCount=*/                  \
         {} _wgpu_COMMA /*.messages=*/{} _wgpu_COMMA})

#define WGPU_COMPILATION_MESSAGE_INIT                                          \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUCompilationMessage,                                                \
        ,                                                                      \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.message=*/NULL                   \
             _wgpu_COMMA /*.type=*/{} _wgpu_COMMA       /*.lineNum=*/          \
         {} _wgpu_COMMA /*.linePos=*/{} _wgpu_COMMA     /*.offset=*/           \
         {} _wgpu_COMMA /*.length=*/{} _wgpu_COMMA      /*.utf16LinePos=*/     \
         {} _wgpu_COMMA /*.utf16Offset=*/{} _wgpu_COMMA /*.utf16Length=*/      \
         {} _wgpu_COMMA})

#define WGPU_COMPUTE_PASS_DESCRIPTOR_INIT                                      \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUComputePassDescriptor,                                             \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.label=*/NULL                     \
             _wgpu_COMMA /*.timestampWrites=*/NULL _wgpu_COMMA})

#define WGPU_COMPUTE_PASS_TIMESTAMP_WRITES_INIT                                \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUComputePassTimestampWrites,                                        \
        {/*.querySet=*/{} _wgpu_COMMA /*.beginningOfPassWriteIndex=*/          \
             WGPU_QUERY_SET_INDEX_UNDEFINED                                    \
                 _wgpu_COMMA /*.endOfPassWriteIndex=*/                         \
                     WGPU_QUERY_SET_INDEX_UNDEFINED _wgpu_COMMA})

#define WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT                                  \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUComputePipelineDescriptor,                                         \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.label=*/NULL                     \
             _wgpu_COMMA /*.layout=*/NULL _wgpu_COMMA /*.compute=*/            \
                 WGPU_COMPUTE_STATE_INIT _wgpu_COMMA})

#define WGPU_COMPUTE_STATE_INIT                                                \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUProgrammableStageDescriptor,                                       \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.module=*/NULL                    \
             _wgpu_COMMA /*.entryPoint=*/NULL                                  \
                 _wgpu_COMMA /*.constantCount=*/0 _wgpu_COMMA /*.constants=*/  \
                     NULL _wgpu_COMMA})

#define WGPU_CONSTANT_ENTRY_INIT                                               \
    WGPU_MAKE_INIT_STRUCT(WGPUConstantEntry,                                   \
                          {/*.nextInChain=*/NULL _wgpu_COMMA /*.key=*/NULL     \
                               _wgpu_COMMA /*.value=*/{} _wgpu_COMMA})

#define WGPU_DEPTH_STENCIL_STATE_INIT                                          \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUDepthStencilState,                                                 \
        {/*.nextInChain=*/NULL                                                 \
             _wgpu_COMMA /*.format=*/{} _wgpu_COMMA /*.depthWriteEnabled=*/    \
         0 _wgpu_COMMA /*.depthCompare=*/WGPUCompareFunction_Undefined         \
             _wgpu_COMMA /*.stencilFront=*/WGPU_STENCIL_FACE_STATE_INIT        \
                 _wgpu_COMMA /*.stencilBack=*/WGPU_STENCIL_FACE_STATE_INIT     \
                     _wgpu_COMMA /*.stencilReadMask=*/                         \
         0xFFFFFFFF _wgpu_COMMA  /*.stencilWriteMask=*/                        \
         0xFFFFFFFF _wgpu_COMMA  /*.depthBias=*/                               \
         0 _wgpu_COMMA           /*.depthBiasSlopeScale=*/                     \
         0.0f _wgpu_COMMA /*.depthBiasClamp=*/0.0f _wgpu_COMMA})

#define WGPU_DEVICE_DESCRIPTOR_INIT                                            \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUDeviceDescriptor,                                                  \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.label=*/NULL                     \
             _wgpu_COMMA /*.requiredFeatureCount=*/                            \
         0 _wgpu_COMMA /*.requiredFeatures=*/NULL                              \
             _wgpu_COMMA /*.requiredLimits=*/NULL                              \
                 _wgpu_COMMA /*.defaultQueue=*/WGPU_QUEUE_DESCRIPTOR_INIT      \
                     _wgpu_COMMA /*.deviceLostCallback=*/NULL                  \
                         _wgpu_COMMA /*.deviceLostUserdata=*/NULL              \
                             _wgpu_COMMA})

#define WGPU_EXTENT_3D_INIT                                                    \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUExtent3D,                                                          \
        {/*.width=*/0 _wgpu_COMMA /*.height=*/                                 \
         1 _wgpu_COMMA /*.depthOrArrayLayers=*/1 _wgpu_COMMA})

#define WGPU_FRAGMENT_STATE_INIT                                               \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUFragmentState,                                                     \
        {/*.nextInChain=*/NULL                                                 \
             _wgpu_COMMA /*.module=*/{} _wgpu_COMMA /*.entryPoint=*/NULL       \
                 _wgpu_COMMA /*.constantCount=*/0 _wgpu_COMMA /*.constants=*/  \
         {} _wgpu_COMMA /*.targetCount=*/{} _wgpu_COMMA       /*.targets=*/    \
         {} _wgpu_COMMA})

#define WGPU_TEXEL_COPY_BUFFER_INFO_INIT                                       \
    WGPU_MAKE_INIT_STRUCT(WGPUTexelCopyBufferInfo,                             \
                          {/*.nextInChain=*/NULL _wgpu_COMMA /*.layout=*/      \
                               WGPU_TEXTURE_DATA_LAYOUT_INIT                   \
                                   _wgpu_COMMA /*.buffer=*/{} _wgpu_COMMA})

#define WGPU_TEXEL_COPY_TEXTURE_INFO_INIT                                      \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUTexelCopyTextureInfo,                                              \
        {/*.nextInChain=*/NULL _wgpu_COMMA          /*.texture=*/              \
         {} _wgpu_COMMA /*.mipLevel=*/0 _wgpu_COMMA /*.origin=*/               \
             WGPU_ORIGIN_3D_INIT _wgpu_COMMA /*.aspect=*/WGPUTextureAspect_All \
                 _wgpu_COMMA})

#define WGPU_INSTANCE_DESCRIPTOR_INIT                                          \
    WGPU_MAKE_INIT_STRUCT(WGPUInstanceDescriptor,                              \
                          {/*.nextInChain=*/NULL _wgpu_COMMA /*.features=*/    \
                               WGPU_INSTANCE_CAPABILITIES_INIT _wgpu_COMMA})

#define WGPU_INSTANCE_CAPABILITIES_INIT                                        \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUInstanceCapabilities,                                              \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.timedWaitAnyEnable=*/            \
         0 _wgpu_COMMA /*.timedWaitAnyMaxCount=*/0 _wgpu_COMMA})

#define WGPU_LIMITS_INIT                                                                                                                                             \
    WGPU_MAKE_INIT_STRUCT(                                                                                                                                           \
        WGPULimits,                                                                                                                                                  \
        {/*.maxTextureDimension1D=*/                                                                                                                                 \
         WGPU_LIMIT_U32_UNDEFINED _wgpu_COMMA /*.maxTextureDimension2D=*/WGPU_LIMIT_U32_UNDEFINED                                                                    \
             _wgpu_COMMA /*.maxTextureDimension3D=*/WGPU_LIMIT_U32_UNDEFINED _wgpu_COMMA /*.maxTextureArrayLayers=*/WGPU_LIMIT_U32_UNDEFINED                         \
                 _wgpu_COMMA /*.maxBindGroups=*/WGPU_LIMIT_U32_UNDEFINED _wgpu_COMMA /*.maxBindGroupsPlusVertexBuffers=*/WGPU_LIMIT_U32_UNDEFINED                    \
                     _wgpu_COMMA /*.maxBindingsPerBindGroup=*/WGPU_LIMIT_U32_UNDEFINED _wgpu_COMMA /*.maxDynamicUniformBuffersPerPipelineLayout=*/                   \
                         WGPU_LIMIT_U32_UNDEFINED _wgpu_COMMA /*.maxDynamicStorageBuffersPerPipelineLayout=*/WGPU_LIMIT_U32_UNDEFINED                                \
                             _wgpu_COMMA /*.maxSampledTexturesPerShaderStage=*/WGPU_LIMIT_U32_UNDEFINED _wgpu_COMMA /*.maxSamplersPerShaderStage=*/                  \
                                 WGPU_LIMIT_U32_UNDEFINED _wgpu_COMMA /*.maxStorageBuffersPerShaderStage=*/WGPU_LIMIT_U32_UNDEFINED                                  \
                                     _wgpu_COMMA /*.maxStorageTexturesPerShaderStage=*/WGPU_LIMIT_U32_UNDEFINED _wgpu_COMMA /*.maxUniformBuffersPerShaderStage=*/    \
                                         WGPU_LIMIT_U32_UNDEFINED _wgpu_COMMA /*.maxUniformBufferBindingSize=*/WGPU_LIMIT_U64_UNDEFINED                              \
                                             _wgpu_COMMA /*.maxStorageBufferBindingSize=*/WGPU_LIMIT_U64_UNDEFINED _wgpu_COMMA /*.minUniformBufferOffsetAlignment=*/ \
                                                 WGPU_LIMIT_U32_UNDEFINED _wgpu_COMMA /*.minStorageBufferOffsetAlignment=*/                                          \
                                                     WGPU_LIMIT_U32_UNDEFINED _wgpu_COMMA /*.maxVertexBuffers=*/WGPU_LIMIT_U32_UNDEFINED                             \
                                                         _wgpu_COMMA /*.maxBufferSize=*/WGPU_LIMIT_U64_UNDEFINED _wgpu_COMMA /*.maxVertexAttributes=*/               \
                                                             WGPU_LIMIT_U32_UNDEFINED _wgpu_COMMA /*.maxVertexBufferArrayStride=*/WGPU_LIMIT_U32_UNDEFINED           \
                                                                 _wgpu_COMMA /*.maxInterStageShaderComponents=*/WGPU_LIMIT_U32_UNDEFINED                             \
                                                                     _wgpu_COMMA /*.maxInterStageShaderVariables=*/WGPU_LIMIT_U32_UNDEFINED                          \
                                                                         _wgpu_COMMA /*.maxColorAttachments=*/WGPU_LIMIT_U32_UNDEFINED                               \
                                                                             _wgpu_COMMA /*.maxColorAttachmentBytesPerSample=*/WGPU_LIMIT_U32_UNDEFINED              \
                                                                                 _wgpu_COMMA /*.maxComputeWorkgroupStorageSize=*/WGPU_LIMIT_U32_UNDEFINED            \
                                                                                     _wgpu_COMMA /*.maxComputeInvocationsPerWorkgroup=*/WGPU_LIMIT_U32_UNDEFINED     \
                                                                                         _wgpu_COMMA /*.maxComputeWorkgroupSizeX=*/WGPU_LIMIT_U32_UNDEFINED          \
                                                                                             _wgpu_COMMA /*.maxComputeWorkgroupSizeY=*/                              \
                                                                                                 WGPU_LIMIT_U32_UNDEFINED                                            \
                                                                                                     _wgpu_COMMA /*.maxComputeWorkgroupSizeZ=*/                      \
                                                                                                         WGPU_LIMIT_U32_UNDEFINED                                    \
                                                                                                             _wgpu_COMMA /*.maxComputeWorkgroupsPerDimension=*/      \
                                                                                                                 WGPU_LIMIT_U32_UNDEFINED                            \
                                                                                                                     _wgpu_COMMA})

#define WGPU_MULTISAMPLE_STATE_INIT                                            \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUMultisampleState,                                                  \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.count=*/1 _wgpu_COMMA /*.mask=*/ \
         0xFFFFFFFF _wgpu_COMMA /*.alphaToCoverageEnabled=*/0 _wgpu_COMMA})

#define WGPU_ORIGIN_3D_INIT                                                    \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUOrigin3D,                                                          \
        {/*.x=*/0 _wgpu_COMMA /*.y=*/0 _wgpu_COMMA /*.z=*/0 _wgpu_COMMA})

#define WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT                                   \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUPipelineLayoutDescriptor,                                          \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.label=*/NULL                     \
             _wgpu_COMMA /*.bindGroupLayoutCount=*/                            \
         {} _wgpu_COMMA /*.bindGroupLayouts=*/NULL _wgpu_COMMA})

#define WGPU_PRIMITIVE_STATE_INIT                                              \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUPrimitiveState,                                                    \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.topology=*/                      \
             WGPUPrimitiveTopology_TriangleList                                \
                 _wgpu_COMMA /*.stripIndexFormat=*/WGPUIndexFormat_Undefined   \
                     _wgpu_COMMA /*.frontFace=*/WGPUFrontFace_CCW              \
                         _wgpu_COMMA /*.cullMode=*/WGPUCullMode_None           \
                             _wgpu_COMMA})

#define WGPU_QUERY_SET_DESCRIPTOR_INIT                                         \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUQuerySetDescriptor,                                                \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.label=*/NULL                     \
             _wgpu_COMMA /*.type=*/{} _wgpu_COMMA /*.count=*/{} _wgpu_COMMA})

#define WGPU_QUEUE_DESCRIPTOR_INIT                                             \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUQueueDescriptor,                                                   \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.label=*/NULL _wgpu_COMMA})

#define WGPU_RENDER_BUNDLE_DESCRIPTOR_INIT                                     \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPURenderBundleDescriptor,                                            \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.label=*/NULL _wgpu_COMMA})

#define WGPU_RENDER_BUNDLE_ENCODER_DESCRIPTOR_INIT                             \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPURenderBundleEncoderDescriptor,                                     \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.label=*/NULL                     \
             _wgpu_COMMA /*.colorFormatCount=*/                                \
         {} _wgpu_COMMA  /*.colorFormats=*/                                    \
         {} _wgpu_COMMA /*.depthStencilFormat=*/WGPUTextureFormat_Undefined    \
             _wgpu_COMMA /*.sampleCount=*/1 _wgpu_COMMA /*.depthReadOnly=*/    \
         0 _wgpu_COMMA /*.stencilReadOnly=*/0 _wgpu_COMMA})

#define WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT                                 \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPURenderPassColorAttachment,                                         \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.view=*/NULL                      \
             _wgpu_COMMA /*.depthSlice=*/WGPU_DEPTH_SLICE_UNDEFINED            \
                 _wgpu_COMMA /*.resolveTarget=*/NULL                           \
                     _wgpu_COMMA /*.loadOp=*/{} _wgpu_COMMA /*.storeOp=*/      \
         {} _wgpu_COMMA /*.clearValue=*/WGPU_COLOR_INIT _wgpu_COMMA})

#define WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT                         \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPURenderPassDepthStencilAttachment,                                  \
        {/*.view=*/{} _wgpu_COMMA /*.depthLoadOp=*/WGPULoadOp_Undefined        \
             _wgpu_COMMA /*.depthStoreOp=*/WGPUStoreOp_Undefined               \
                 _wgpu_COMMA /*.depthClearValue=*/                             \
                     WGPU_DEPTH_CLEAR_VALUE_UNDEFINED                          \
                         _wgpu_COMMA /*.depthReadOnly=*/                       \
         0 _wgpu_COMMA /*.stencilLoadOp=*/WGPULoadOp_Undefined                 \
             _wgpu_COMMA /*.stencilStoreOp=*/WGPUStoreOp_Undefined             \
                 _wgpu_COMMA /*.stencilClearValue=*/                           \
         0 _wgpu_COMMA /*.stencilReadOnly=*/0 _wgpu_COMMA})

#define WGPU_RENDER_PASS_DESCRIPTOR_INIT                                       \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPURenderPassDescriptor,                                              \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.label=*/NULL                     \
             _wgpu_COMMA /*.colorAttachmentCount=*/                            \
         {} _wgpu_COMMA  /*.colorAttachments=*/                                \
         {} _wgpu_COMMA /*.depthStencilAttachment=*/NULL                       \
             _wgpu_COMMA /*.occlusionQuerySet=*/NULL                           \
                 _wgpu_COMMA /*.timestampWrites=*/NULL _wgpu_COMMA})

#define WGPU_RENDER_PASS_MAX_DRAW_COUNT_INIT                                   \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPURenderPassMaxDrawCount,                                            \
        {/*.chain= WGPUChainedStruct*/ {                                       \
            /*.next=*/NULL _wgpu_COMMA /*.sType=*/                             \
                WGPUSType_RenderPassDescriptorMaxDrawCount                     \
                    _wgpu_COMMA} _wgpu_COMMA /*.maxDrawCount=*/                \
         50000000 _wgpu_COMMA})

#define WGPU_RENDER_PASS_TIMESTAMP_WRITES_INIT                                 \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPURenderPassTimestampWrites,                                         \
        {/*.querySet=*/{} _wgpu_COMMA /*.beginningOfPassWriteIndex=*/          \
             WGPU_QUERY_SET_INDEX_UNDEFINED                                    \
                 _wgpu_COMMA /*.endOfPassWriteIndex=*/                         \
                     WGPU_QUERY_SET_INDEX_UNDEFINED _wgpu_COMMA})

#define WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT                                   \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPURenderPipelineDescriptor,                                          \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.label=*/NULL                     \
             _wgpu_COMMA /*.layout=*/NULL _wgpu_COMMA      /*.vertex=*/        \
                 WGPU_VERTEX_STATE_INIT _wgpu_COMMA        /*.primitive=*/     \
                     WGPU_PRIMITIVE_STATE_INIT _wgpu_COMMA /*.depthStencil=*/  \
                         NULL _wgpu_COMMA                  /*.multisample=*/   \
                             WGPU_MULTISAMPLE_STATE_INIT                       \
                                 _wgpu_COMMA /*.fragment=*/NULL _wgpu_COMMA})

#define WGPU_REQUEST_ADAPTER_OPTIONS_INIT                                      \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPURequestAdapterOptions,                                             \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.compatibleSurface=*/NULL         \
             _wgpu_COMMA /*.powerPreference=*/WGPUPowerPreference_Undefined    \
                 _wgpu_COMMA /*.backendType=*/WGPUBackendType_Undefined        \
                     _wgpu_COMMA /*.forceFallbackAdapter=*/                    \
         0 _wgpu_COMMA /*.compatibilityMode=*/0 _wgpu_COMMA})

#define WGPU_REQUIRED_LIMITS_INIT                                              \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPURequiredLimits,                                                    \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.limits=*/WGPU_LIMITS_INIT        \
             _wgpu_COMMA})

#define WGPU_SAMPLER_BINDING_LAYOUT_INIT                                       \
    WGPU_MAKE_INIT_STRUCT(WGPUSamplerBindingLayout,                            \
                          {/*.nextInChain=*/NULL _wgpu_COMMA /*.type=*/        \
                               WGPUSamplerBindingType_Undefined _wgpu_COMMA})

#define WGPU_SAMPLER_DESCRIPTOR_INIT                                           \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUSamplerDescriptor,                                                 \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.label=*/NULL                     \
             _wgpu_COMMA /*.addressModeU=*/WGPUAddressMode_ClampToEdge         \
                 _wgpu_COMMA /*.addressModeV=*/WGPUAddressMode_ClampToEdge     \
                     _wgpu_COMMA /*.addressModeW=*/WGPUAddressMode_ClampToEdge \
                         _wgpu_COMMA /*.magFilter=*/WGPUFilterMode_Nearest     \
                             _wgpu_COMMA /*.minFilter=*/WGPUFilterMode_Nearest \
                                 _wgpu_COMMA /*.mipmapFilter=*/                \
                                     WGPUMipmapFilterMode_Nearest              \
                                         _wgpu_COMMA         /*.lodMinClamp=*/ \
         0.0f _wgpu_COMMA /*.lodMaxClamp=*/32.0f _wgpu_COMMA /*.compare=*/     \
             WGPUCompareFunction_Undefined                                     \
                 _wgpu_COMMA /*.maxAnisotropy=*/1 _wgpu_COMMA})

#define WGPU_SHADER_MODULE_DESCRIPTOR_INIT                                     \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUShaderModuleDescriptor,                                            \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.label=*/NULL _wgpu_COMMA})

#define WGPU_SHADER_SOURCE_SPIRV_INIT                                          \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUShaderSourceSPIRV,                                                 \
        {/*.chain= WGPUChainedStruct*/ {                                       \
             /*.next=*/NULL _wgpu_COMMA /*.sType=*/WGPUSType_ShaderSourceSPIRV \
                 _wgpu_COMMA} _wgpu_COMMA /*.codeSize=*/{},                    \
         .code = {} _wgpu_COMMA})

#define WGPU_SHADER_SOURCE_WGSL_INIT                                           \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUShaderSourceWGSL,                                                  \
        {/*.chain= WGPUChainedStruct*/ {                                       \
            /*.next=*/NULL _wgpu_COMMA /*.sType=*/WGPUSType_ShaderSourceWGSL   \
                _wgpu_COMMA} _wgpu_COMMA /*.code=*/NULL _wgpu_COMMA})

#define WGPU_STENCIL_FACE_STATE_INIT                                           \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUStencilFaceState,                                                  \
        {/*.compare=*/WGPUCompareFunction_Always _wgpu_COMMA /*.failOp=*/      \
             WGPUStencilOperation_Keep                                         \
                 _wgpu_COMMA /*.depthFailOp=*/WGPUStencilOperation_Keep        \
                     _wgpu_COMMA /*.passOp=*/WGPUStencilOperation_Keep         \
                         _wgpu_COMMA})

#define WGPU_STORAGE_TEXTURE_BINDING_LAYOUT_INIT                               \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUStorageTextureBindingLayout,                                       \
        {/*.nextInChain=*/NULL _wgpu_COMMA                  /*.access=*/       \
             WGPUStorageTextureAccess_Undefined _wgpu_COMMA /*.format=*/       \
                 WGPUTextureFormat_Undefined _wgpu_COMMA /*.viewDimension=*/   \
                     WGPUTextureViewDimension_Undefined _wgpu_COMMA})

#define WGPU_SUPPORTED_LIMITS_INIT                                             \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUSupportedLimits,                                                   \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.limits=*/WGPU_LIMITS_INIT        \
             _wgpu_COMMA})

#define WGPU_SURFACE_CAPABILITIES_INIT                                         \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUSurfaceCapabilities,                                               \
        {/*.nextInChain=*/NULL _wgpu_COMMA          /*.formatCount=*/          \
         {} _wgpu_COMMA /*.formats=*/{} _wgpu_COMMA /*.presentModeCount=*/     \
         {} _wgpu_COMMA /*.presentModes=*/{} _wgpu_COMMA /*.alphaModeCount=*/  \
         {} _wgpu_COMMA /*.alphaModes=*/{} _wgpu_COMMA})

#define WGPU_SURFACE_CONFIGURATION_INIT                                        \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUSurfaceConfiguration,                                              \
        {/*.nextInChain=*/NULL                                                 \
             _wgpu_COMMA /*.device=*/{} _wgpu_COMMA /*.format=*/               \
         {} _wgpu_COMMA /*.usage=*/WGPUTextureUsage_RenderAttachment           \
             _wgpu_COMMA /*.viewFormatCount=*/0 _wgpu_COMMA /*.viewFormats=*/  \
                 NULL _wgpu_COMMA /*.alphaMode=*/WGPUCompositeAlphaMode_Auto   \
                     _wgpu_COMMA /*.width=*/{} _wgpu_COMMA /*.height=*/        \
         {} _wgpu_COMMA /*.presentMode=*/WGPUPresentMode_Fifo _wgpu_COMMA})

#define WGPU_SURFACE_DESCRIPTOR_INIT                                           \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUSurfaceDescriptor,                                                 \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.label=*/NULL _wgpu_COMMA})

#define WGPU_SURFACE_TEXTURE_INIT                                              \
    WGPU_MAKE_INIT_STRUCT(WGPUSurfaceTexture,                                  \
                          {/*.texture=*/{} _wgpu_COMMA /*.suboptimal=*/        \
                           {} _wgpu_COMMA /*.status=*/{} _wgpu_COMMA})

#define WGPU_TEXTURE_BINDING_LAYOUT_INIT                                       \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUTextureBindingLayout,                                              \
        {/*.nextInChain=*/NULL _wgpu_COMMA               /*.sampleType=*/      \
             WGPUTextureSampleType_Undefined _wgpu_COMMA /*.viewDimension=*/   \
                 WGPUTextureViewDimension_Undefined                            \
                     _wgpu_COMMA /*.multisampled=*/0 _wgpu_COMMA})

#define WGPU_TEXTURE_BINDING_VIEW_DIMENSION_DESCRIPTOR_INIT                    \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUTextureBindingViewDimensionDescriptor,                             \
        {/*.chain= WGPUChainedStruct*/ {                                       \
            /*.next=*/NULL _wgpu_COMMA /*.sType=*/                             \
                WGPUSType_TextureBindingViewDimensionDescriptor                \
                    _wgpu_COMMA} _wgpu_COMMA /*.textureBindingViewDimension=*/ \
             WGPUTextureViewDimension_Undefined _wgpu_COMMA})

#define WGPU_TEXTURE_DATA_LAYOUT_INIT                                          \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUTextureDataLayout,                                                 \
        {/*.nextInChain=*/NULL                                                 \
             _wgpu_COMMA /*.offset=*/0 _wgpu_COMMA      /*.bytesPerRow=*/      \
                 WGPU_COPY_STRIDE_UNDEFINED _wgpu_COMMA /*.rowsPerImage=*/     \
                     WGPU_COPY_STRIDE_UNDEFINED _wgpu_COMMA})

#define WGPU_TEXTURE_DESCRIPTOR_INIT                                           \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUTextureDescriptor,                                                 \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.label=*/NULL                     \
             _wgpu_COMMA /*.usage=*/{} _wgpu_COMMA        /*.dimension=*/      \
                 WGPUTextureDimension_2D _wgpu_COMMA      /*.size=*/           \
                     WGPU_EXTENT_3D_INIT _wgpu_COMMA      /*.format=*/         \
         {} _wgpu_COMMA /*.mipLevelCount=*/1 _wgpu_COMMA  /*.sampleCount=*/    \
         1 _wgpu_COMMA /*.viewFormatCount=*/0 _wgpu_COMMA /*.viewFormats=*/    \
             NULL _wgpu_COMMA})

#define WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT                                      \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUTextureViewDescriptor,                                             \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.label=*/NULL                     \
             _wgpu_COMMA /*.format=*/WGPUTextureFormat_Undefined               \
                 _wgpu_COMMA /*.dimension=*/WGPUTextureViewDimension_Undefined \
                     _wgpu_COMMA /*.baseMipLevel=*/                            \
         0 _wgpu_COMMA /*.mipLevelCount=*/WGPU_MIP_LEVEL_COUNT_UNDEFINED       \
             _wgpu_COMMA /*.baseArrayLayer=*/                                  \
         0 _wgpu_COMMA /*.arrayLayerCount=*/WGPU_ARRAY_LAYER_COUNT_UNDEFINED   \
             _wgpu_COMMA /*.aspect=*/WGPUTextureAspect_All _wgpu_COMMA})

#define WGPU_VERTEX_ATTRIBUTE_INIT                                             \
    WGPU_MAKE_INIT_STRUCT(WGPUVertexAttribute,                                 \
                          {/*.format=*/{} _wgpu_COMMA /*.offset=*/             \
                           {} _wgpu_COMMA /*.shaderLocation=*/{} _wgpu_COMMA})

#define WGPU_VERTEX_BUFFER_LAYOUT_INIT                                         \
    WGPU_MAKE_INIT_STRUCT(WGPUVertexBufferLayout,                              \
                          {/*.arrayStride=*/{} _wgpu_COMMA /*.stepMode=*/      \
                           {} _wgpu_COMMA /*.attributeCount=*/                 \
                           {} _wgpu_COMMA /*.attributes=*/{} _wgpu_COMMA})

#define WGPU_VERTEX_STATE_INIT                                                 \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUVertexState,                                                       \
        {/*.nextInChain=*/NULL                                                 \
             _wgpu_COMMA /*.module=*/{} _wgpu_COMMA /*.entryPoint=*/NULL       \
                 _wgpu_COMMA /*.constantCount=*/0 _wgpu_COMMA /*.constants=*/  \
         {} _wgpu_COMMA /*.bufferCount=*/0 _wgpu_COMMA        /*.buffers=*/    \
         {} _wgpu_COMMA})

#define WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT                                \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUQueueWorkDoneCallbackInfo,                                         \
        {/*.nextInChain=*/NULL _wgpu_COMMA /*.callback=*/NULL                  \
             _wgpu_COMMA /*.userdata1=*/NULL _wgpu_COMMA /*.userdata2=*/NULL   \
                 _wgpu_COMMA})

#if defined(__cplusplus)
#define WGPU_WAGYU_STRING_VIEW(s)                                              \
    WGPUWagyuStringView{s, (s != NULL) ? strlen(s) : 0}
#else
#define WGPU_WAGYU_STRING_VIEW(s)                                              \
    (WGPUWagyuStringView){s, (s != NULL) ? strlen(s) : 0}
#endif

#endif // WEBGPU_COMPAT_H
