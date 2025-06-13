#ifndef WEBGPU_COMPAT_H
#define WEBGPU_COMPAT_H

#define WGPUTexelCopyBufferInfo WGPUImageCopyBuffer
#define WGPUTexelCopyTextureInfo WGPUImageCopyTexture
#define WGPUInstanceCapabilities WGPUInstanceFeatures

#if !defined(WGPUOptionalBool)
#define WGPUOptionalBool int32_t
#define WGPUOptionalBool_False 0x00000000
#define WGPUOptionalBool_True 0x00000001
#define WGPUOptionalBool_Undefined 0x00000002
#define WGPUOptionalBool_Force32 0x7FFFFFFF
#endif

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

#define WGPU_COMMA ,

#define WGPU_ADAPTER_INFO_INIT                                                 \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUAdapterInfo,                                                       \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.vendor=*/NULL                     \
             WGPU_COMMA /*.architecture=*/NULL WGPU_COMMA /*.device=*/NULL     \
                 WGPU_COMMA /*.description=*/NULL WGPU_COMMA /*.backendType=*/ \
         {} WGPU_COMMA /*.adapterType=*/{} WGPU_COMMA        /*.vendorID=*/    \
         {} WGPU_COMMA /*.deviceID=*/{} WGPU_COMMA})

#define WGPU_BIND_GROUP_DESCRIPTOR_INIT                                        \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUBindGroupDescriptor,                                               \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.label=*/NULL                      \
             WGPU_COMMA /*.layout=*/{} WGPU_COMMA /*.entryCount=*/             \
         {} WGPU_COMMA /*.entries=*/{} WGPU_COMMA})

#define WGPU_BIND_GROUP_ENTRY_INIT                                             \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUBindGroupEntry,                                                    \
        {/*.nextInChain=*/NULL                                                 \
             WGPU_COMMA /*.binding=*/{} WGPU_COMMA /*.buffer=*/NULL            \
                 WGPU_COMMA /*.offset=*/0 WGPU_COMMA /*.size=*/WGPU_WHOLE_SIZE \
                     WGPU_COMMA /*.sampler=*/NULL                              \
                         WGPU_COMMA /*.textureView=*/NULL WGPU_COMMA})

#define WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT                                 \
    WGPU_MAKE_INIT_STRUCT(WGPUBindGroupLayoutDescriptor,                       \
                          {/*.nextInChain=*/NULL WGPU_COMMA /*.label=*/NULL    \
                               WGPU_COMMA /*.entryCount=*/                     \
                           {} WGPU_COMMA /*.entries=*/{} WGPU_COMMA})

#define WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT                                      \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUBindGroupLayoutEntry,                                              \
        {/*.nextInChain=*/NULL                                                 \
             WGPU_COMMA /*.binding=*/{} WGPU_COMMA /*.visibility=*/            \
         {} WGPU_COMMA /*.buffer=*/WGPU_BUFFER_BINDING_LAYOUT_INIT             \
             WGPU_COMMA /*.sampler=*/WGPU_SAMPLER_BINDING_LAYOUT_INIT          \
                 WGPU_COMMA /*.texture=*/WGPU_TEXTURE_BINDING_LAYOUT_INIT      \
                     WGPU_COMMA /*.storageTexture=*/                           \
                         WGPU_STORAGE_TEXTURE_BINDING_LAYOUT_INIT WGPU_COMMA})

#define WGPU_BLEND_COMPONENT_INIT                                              \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUBlendComponent,                                                    \
        {/*.operation=*/WGPUBlendOperation_Add WGPU_COMMA /*.srcFactor=*/      \
             WGPUBlendFactor_One                                               \
                 WGPU_COMMA /*.dstFactor=*/WGPUBlendFactor_Zero WGPU_COMMA})

#define WGPU_BLEND_STATE_INIT                                                  \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUBlendState,                                                        \
        {/*.color=*/WGPU_BLEND_COMPONENT_INIT WGPU_COMMA /*.alpha=*/           \
             WGPU_BLEND_COMPONENT_INIT WGPU_COMMA})

#define WGPU_BUFFER_BINDING_LAYOUT_INIT                                        \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUBufferBindingLayout,                                               \
        {/*.nextInChain=*/NULL WGPU_COMMA             /*.type=*/               \
             WGPUBufferBindingType_Uniform WGPU_COMMA /*.hasDynamicOffset=*/   \
         false WGPU_COMMA /*.minBindingSize=*/0 WGPU_COMMA})

#define WGPU_BUFFER_DESCRIPTOR_INIT                                            \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUBufferDescriptor,                                                  \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.label=*/NULL                      \
             WGPU_COMMA /*.usage=*/{} WGPU_COMMA /*.size=*/                    \
         {} WGPU_COMMA /*.mappedAtCreation=*/false WGPU_COMMA})

#define WGPU_BUFFER_MAP_CALLBACK_INFO_INIT                                     \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUBufferMapCallbackInfo,                                             \
        {/*.nextInChain=*/NULL                                                 \
             WGPU_COMMA /*.mode=*/{} WGPU_COMMA /*.callback=*/NULL             \
                 WGPU_COMMA /*.userdata1=*/NULL WGPU_COMMA /*.userdata2=*/NULL \
                     WGPU_COMMA})

#define WGPU_COLOR_INIT                                                        \
    WGPU_MAKE_INIT_STRUCT(WGPUColor,                                           \
                          {/*.r=*/{} WGPU_COMMA /*.g=*/{} WGPU_COMMA /*.b=*/   \
                           {} WGPU_COMMA /*.a=*/{} WGPU_COMMA})

#define WGPU_COLOR_TARGET_STATE_INIT                                           \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUColorTargetState,                                                  \
        {/*.nextInChain=*/NULL                                                 \
             WGPU_COMMA /*.format=*/{} WGPU_COMMA /*.blend=*/NULL              \
                 WGPU_COMMA /*.writeMask=*/WGPUColorWriteMask_All WGPU_COMMA})

#define WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT                                    \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUCommandBufferDescriptor,                                           \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.label=*/NULL WGPU_COMMA})

#define WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT                                   \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUCommandEncoderDescriptor,                                          \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.label=*/NULL WGPU_COMMA})

#define WGPU_COMPILATION_INFO_INIT                                             \
    WGPU_MAKE_INIT_STRUCT(WGPUCompilationInfo,                                 \
                          {/*.nextInChain=*/NULL WGPU_COMMA /*.messageCount=*/ \
                           {} WGPU_COMMA /*.messages=*/{} WGPU_COMMA})

#define WGPU_COMPILATION_MESSAGE_INIT                                          \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUCompilationMessage,                                                \
        ,                                                                      \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.message=*/NULL                    \
             WGPU_COMMA /*.type=*/{} WGPU_COMMA       /*.lineNum=*/            \
         {} WGPU_COMMA /*.linePos=*/{} WGPU_COMMA     /*.offset=*/             \
         {} WGPU_COMMA /*.length=*/{} WGPU_COMMA      /*.utf16LinePos=*/       \
         {} WGPU_COMMA /*.utf16Offset=*/{} WGPU_COMMA /*.utf16Length=*/        \
         {} WGPU_COMMA})

#define WGPU_COMPUTE_PASS_DESCRIPTOR_INIT                                      \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUComputePassDescriptor,                                             \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.label=*/NULL                      \
             WGPU_COMMA /*.timestampWrites=*/NULL WGPU_COMMA})

#define WGPU_COMPUTE_PASS_TIMESTAMP_WRITES_INIT                                \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUComputePassTimestampWrites,                                        \
        {/*.querySet=*/{} WGPU_COMMA /*.beginningOfPassWriteIndex=*/           \
             WGPU_QUERY_SET_INDEX_UNDEFINED                                    \
                 WGPU_COMMA /*.endOfPassWriteIndex=*/                          \
                     WGPU_QUERY_SET_INDEX_UNDEFINED WGPU_COMMA})

#define WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT                                  \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUComputePipelineDescriptor,                                         \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.label=*/NULL                      \
             WGPU_COMMA /*.layout=*/NULL                                       \
                 WGPU_COMMA /*.compute=*/WGPU_COMPUTE_STATE_INIT WGPU_COMMA})

#define WGPU_COMPUTE_STATE_INIT                                                \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUProgrammableStageDescriptor,                                       \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.module=*/NULL                     \
             WGPU_COMMA /*.entryPoint=*/NULL                                   \
                 WGPU_COMMA /*.constantCount=*/0 WGPU_COMMA /*.constants=*/    \
                     NULL WGPU_COMMA})

#define WGPU_CONSTANT_ENTRY_INIT                                               \
    WGPU_MAKE_INIT_STRUCT(WGPUConstantEntry,                                   \
                          {/*.nextInChain=*/NULL WGPU_COMMA /*.key=*/NULL      \
                               WGPU_COMMA /*.value=*/{} WGPU_COMMA})

#define WGPU_DEPTH_STENCIL_STATE_INIT                                          \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUDepthStencilState,                                                 \
        {/*.nextInChain=*/NULL                                                 \
             WGPU_COMMA /*.format=*/{} WGPU_COMMA /*.depthWriteEnabled=*/      \
         false WGPU_COMMA /*.depthCompare=*/WGPUCompareFunction_Undefined      \
             WGPU_COMMA /*.stencilFront=*/WGPU_STENCIL_FACE_STATE_INIT         \
                 WGPU_COMMA /*.stencilBack=*/WGPU_STENCIL_FACE_STATE_INIT      \
                     WGPU_COMMA /*.stencilReadMask=*/                          \
         0xFFFFFFFF WGPU_COMMA  /*.stencilWriteMask=*/                         \
         0xFFFFFFFF WGPU_COMMA  /*.depthBias=*/                                \
         0 WGPU_COMMA           /*.depthBiasSlopeScale=*/                      \
         0.0f WGPU_COMMA /*.depthBiasClamp=*/0.0f WGPU_COMMA})

#define WGPU_DEVICE_DESCRIPTOR_INIT                                            \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUDeviceDescriptor,                                                  \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.label=*/NULL                      \
             WGPU_COMMA /*.requiredFeatureCount=*/                             \
         0 WGPU_COMMA /*.requiredFeatures=*/NULL                               \
             WGPU_COMMA /*.requiredLimits=*/NULL                               \
                 WGPU_COMMA /*.defaultQueue=*/WGPU_QUEUE_DESCRIPTOR_INIT       \
                     WGPU_COMMA /*.deviceLostCallback=*/NULL                   \
                         WGPU_COMMA /*.deviceLostUserdata=*/NULL WGPU_COMMA})

#define WGPU_EXTENT_3D_INIT                                                    \
    WGPU_MAKE_INIT_STRUCT(WGPUExtent3D,                                        \
                          {/*.width=*/0 WGPU_COMMA /*.height=*/                \
                           1 WGPU_COMMA /*.depthOrArrayLayers=*/1 WGPU_COMMA})

#define WGPU_FRAGMENT_STATE_INIT                                               \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUFragmentState,                                                     \
        {/*.nextInChain=*/NULL                                                 \
             WGPU_COMMA /*.module=*/{} WGPU_COMMA /*.entryPoint=*/NULL         \
                 WGPU_COMMA /*.constantCount=*/0 WGPU_COMMA /*.constants=*/    \
         {} WGPU_COMMA /*.targetCount=*/{} WGPU_COMMA       /*.targets=*/      \
         {} WGPU_COMMA})

#define WGPU_TEXEL_COPY_BUFFER_INFO_INIT                                       \
    WGPU_MAKE_INIT_STRUCT(WGPUTexelCopyBufferInfo,                             \
                          {/*.nextInChain=*/NULL WGPU_COMMA /*.layout=*/       \
                               WGPU_TEXTURE_DATA_LAYOUT_INIT                   \
                                   WGPU_COMMA /*.buffer=*/{} WGPU_COMMA})

#define WGPU_TEXEL_COPY_TEXTURE_INFO_INIT                                      \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUTexelCopyTextureInfo,                                              \
        {/*.nextInChain=*/NULL WGPU_COMMA         /*.texture=*/                \
         {} WGPU_COMMA /*.mipLevel=*/0 WGPU_COMMA /*.origin=*/                 \
             WGPU_ORIGIN_3D_INIT WGPU_COMMA /*.aspect=*/WGPUTextureAspect_All  \
                 WGPU_COMMA})

#define WGPU_INSTANCE_DESCRIPTOR_INIT                                          \
    WGPU_MAKE_INIT_STRUCT(WGPUInstanceDescriptor,                              \
                          {/*.nextInChain=*/NULL WGPU_COMMA /*.features=*/     \
                               WGPU_INSTANCE_CAPABILITIES_INIT WGPU_COMMA})

#define WGPU_INSTANCE_CAPABILITIES_INIT                                        \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUInstanceCapabilities,                                              \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.timedWaitAnyEnable=*/             \
         false WGPU_COMMA /*.timedWaitAnyMaxCount=*/0 WGPU_COMMA})

#define WGPU_LIMITS_INIT                                                                                                                                           \
    WGPU_MAKE_INIT_STRUCT(                                                                                                                                         \
        WGPULimits,                                                                                                                                                \
        {/*.maxTextureDimension1D=*/                                                                                                                               \
         WGPU_LIMIT_U32_UNDEFINED WGPU_COMMA /*.maxTextureDimension2D=*/WGPU_LIMIT_U32_UNDEFINED                                                                   \
             WGPU_COMMA /*.maxTextureDimension3D=*/WGPU_LIMIT_U32_UNDEFINED WGPU_COMMA /*.maxTextureArrayLayers=*/WGPU_LIMIT_U32_UNDEFINED                         \
                 WGPU_COMMA /*.maxBindGroups=*/WGPU_LIMIT_U32_UNDEFINED WGPU_COMMA /*.maxBindGroupsPlusVertexBuffers=*/WGPU_LIMIT_U32_UNDEFINED                    \
                     WGPU_COMMA /*.maxBindingsPerBindGroup=*/WGPU_LIMIT_U32_UNDEFINED WGPU_COMMA /*.maxDynamicUniformBuffersPerPipelineLayout=*/                   \
                         WGPU_LIMIT_U32_UNDEFINED WGPU_COMMA /*.maxDynamicStorageBuffersPerPipelineLayout=*/WGPU_LIMIT_U32_UNDEFINED                               \
                             WGPU_COMMA /*.maxSampledTexturesPerShaderStage=*/WGPU_LIMIT_U32_UNDEFINED WGPU_COMMA /*.maxSamplersPerShaderStage=*/                  \
                                 WGPU_LIMIT_U32_UNDEFINED WGPU_COMMA /*.maxStorageBuffersPerShaderStage=*/WGPU_LIMIT_U32_UNDEFINED                                 \
                                     WGPU_COMMA /*.maxStorageTexturesPerShaderStage=*/WGPU_LIMIT_U32_UNDEFINED WGPU_COMMA /*.maxUniformBuffersPerShaderStage=*/    \
                                         WGPU_LIMIT_U32_UNDEFINED WGPU_COMMA /*.maxUniformBufferBindingSize=*/WGPU_LIMIT_U64_UNDEFINED                             \
                                             WGPU_COMMA /*.maxStorageBufferBindingSize=*/WGPU_LIMIT_U64_UNDEFINED WGPU_COMMA /*.minUniformBufferOffsetAlignment=*/ \
                                                 WGPU_LIMIT_U32_UNDEFINED WGPU_COMMA /*.minStorageBufferOffsetAlignment=*/                                         \
                                                     WGPU_LIMIT_U32_UNDEFINED WGPU_COMMA /*.maxVertexBuffers=*/WGPU_LIMIT_U32_UNDEFINED                            \
                                                         WGPU_COMMA /*.maxBufferSize=*/WGPU_LIMIT_U64_UNDEFINED WGPU_COMMA /*.maxVertexAttributes=*/               \
                                                             WGPU_LIMIT_U32_UNDEFINED WGPU_COMMA /*.maxVertexBufferArrayStride=*/WGPU_LIMIT_U32_UNDEFINED          \
                                                                 WGPU_COMMA /*.maxInterStageShaderComponents=*/WGPU_LIMIT_U32_UNDEFINED                            \
                                                                     WGPU_COMMA /*.maxInterStageShaderVariables=*/WGPU_LIMIT_U32_UNDEFINED                         \
                                                                         WGPU_COMMA /*.maxColorAttachments=*/WGPU_LIMIT_U32_UNDEFINED                              \
                                                                             WGPU_COMMA /*.maxColorAttachmentBytesPerSample=*/WGPU_LIMIT_U32_UNDEFINED             \
                                                                                 WGPU_COMMA /*.maxComputeWorkgroupStorageSize=*/WGPU_LIMIT_U32_UNDEFINED           \
                                                                                     WGPU_COMMA /*.maxComputeInvocationsPerWorkgroup=*/WGPU_LIMIT_U32_UNDEFINED    \
                                                                                         WGPU_COMMA /*.maxComputeWorkgroupSizeX=*/WGPU_LIMIT_U32_UNDEFINED         \
                                                                                             WGPU_COMMA /*.maxComputeWorkgroupSizeY=*/                             \
                                                                                                 WGPU_LIMIT_U32_UNDEFINED                                          \
                                                                                                     WGPU_COMMA /*.maxComputeWorkgroupSizeZ=*/                     \
                                                                                                         WGPU_LIMIT_U32_UNDEFINED                                  \
                                                                                                             WGPU_COMMA /*.maxComputeWorkgroupsPerDimension=*/     \
                                                                                                                 WGPU_LIMIT_U32_UNDEFINED                          \
                                                                                                                     WGPU_COMMA})

#define WGPU_MULTISAMPLE_STATE_INIT                                            \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUMultisampleState,                                                  \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.count=*/1 WGPU_COMMA /*.mask=*/   \
         0xFFFFFFFF WGPU_COMMA /*.alphaToCoverageEnabled=*/false WGPU_COMMA})

#define WGPU_ORIGIN_3D_INIT                                                    \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUOrigin3D,                                                          \
        {/*.x=*/0 WGPU_COMMA /*.y=*/0 WGPU_COMMA /*.z=*/0 WGPU_COMMA})

#define WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT                                   \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUPipelineLayoutDescriptor,                                          \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.label=*/NULL                      \
             WGPU_COMMA /*.bindGroupLayoutCount=*/                             \
         {} WGPU_COMMA /*.bindGroupLayouts=*/NULL WGPU_COMMA})

#define WGPU_PRIMITIVE_STATE_INIT                                              \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUPrimitiveState,                                                    \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.topology=*/                       \
             WGPUPrimitiveTopology_TriangleList                                \
                 WGPU_COMMA /*.stripIndexFormat=*/WGPUIndexFormat_Undefined    \
                     WGPU_COMMA /*.frontFace=*/WGPUFrontFace_CCW               \
                         WGPU_COMMA /*.cullMode=*/WGPUCullMode_None            \
                             WGPU_COMMA})

#define WGPU_QUERY_SET_DESCRIPTOR_INIT                                         \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUQuerySetDescriptor,                                                \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.label=*/NULL                      \
             WGPU_COMMA /*.type=*/{} WGPU_COMMA /*.count=*/{} WGPU_COMMA})

#define WGPU_QUEUE_DESCRIPTOR_INIT                                             \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUQueueDescriptor,                                                   \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.label=*/NULL WGPU_COMMA})

#define WGPU_RENDER_BUNDLE_DESCRIPTOR_INIT                                     \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPURenderBundleDescriptor,                                            \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.label=*/NULL WGPU_COMMA})

#define WGPU_RENDER_BUNDLE_ENCODER_DESCRIPTOR_INIT                             \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPURenderBundleEncoderDescriptor,                                     \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.label=*/NULL                      \
             WGPU_COMMA /*.colorFormatCount=*/{} WGPU_COMMA /*.colorFormats=*/ \
         {} WGPU_COMMA /*.depthStencilFormat=*/WGPUTextureFormat_Undefined     \
             WGPU_COMMA /*.sampleCount=*/1 WGPU_COMMA /*.depthReadOnly=*/      \
         false WGPU_COMMA /*.stencilReadOnly=*/false WGPU_COMMA})

#define WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT                                 \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPURenderPassColorAttachment,                                         \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.view=*/NULL                       \
             WGPU_COMMA /*.depthSlice=*/WGPU_DEPTH_SLICE_UNDEFINED             \
                 WGPU_COMMA /*.resolveTarget=*/NULL                            \
                     WGPU_COMMA /*.loadOp=*/{} WGPU_COMMA /*.storeOp=*/        \
         {} WGPU_COMMA /*.clearValue=*/WGPU_COLOR_INIT WGPU_COMMA})

#define WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT                         \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPURenderPassDepthStencilAttachment,                                  \
        {/*.view=*/{} WGPU_COMMA /*.depthLoadOp=*/WGPULoadOp_Undefined         \
             WGPU_COMMA /*.depthStoreOp=*/WGPUStoreOp_Undefined                \
                 WGPU_COMMA /*.depthClearValue=*/NAN                           \
                     WGPU_COMMA /*.depthReadOnly=*/                            \
         false WGPU_COMMA /*.stencilLoadOp=*/WGPULoadOp_Undefined              \
             WGPU_COMMA /*.stencilStoreOp=*/WGPUStoreOp_Undefined              \
                 WGPU_COMMA /*.stencilClearValue=*/                            \
         0 WGPU_COMMA /*.stencilReadOnly=*/false WGPU_COMMA})

#define WGPU_RENDER_PASS_DESCRIPTOR_INIT                                       \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPURenderPassDescriptor,                                              \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.label=*/NULL                      \
             WGPU_COMMA /*.colorAttachmentCount=*/                             \
         {} WGPU_COMMA  /*.colorAttachments=*/                                 \
         {} WGPU_COMMA /*.depthStencilAttachment=*/NULL                        \
             WGPU_COMMA /*.occlusionQuerySet=*/NULL                            \
                 WGPU_COMMA /*.timestampWrites=*/NULL WGPU_COMMA})

#define WGPU_RENDER_PASS_MAX_DRAW_COUNT_INIT                                   \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPURenderPassMaxDrawCount,                                            \
        {/*.chain= WGPUChainedStruct*/ {                                       \
            /*.next=*/NULL WGPU_COMMA /*.sType=*/                              \
                WGPUSType_RenderPassDescriptorMaxDrawCount                     \
                    WGPU_COMMA} WGPU_COMMA /*.maxDrawCount=*/                  \
         50000000 WGPU_COMMA})

#define WGPU_RENDER_PASS_TIMESTAMP_WRITES_INIT                                 \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPURenderPassTimestampWrites,                                         \
        {/*.querySet=*/{} WGPU_COMMA /*.beginningOfPassWriteIndex=*/           \
             WGPU_QUERY_SET_INDEX_UNDEFINED                                    \
                 WGPU_COMMA /*.endOfPassWriteIndex=*/                          \
                     WGPU_QUERY_SET_INDEX_UNDEFINED WGPU_COMMA})

#define WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT                                   \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPURenderPipelineDescriptor,                                          \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.label=*/NULL                      \
             WGPU_COMMA /*.layout=*/NULL WGPU_COMMA       /*.vertex=*/         \
                 WGPU_VERTEX_STATE_INIT WGPU_COMMA        /*.primitive=*/      \
                     WGPU_PRIMITIVE_STATE_INIT WGPU_COMMA /*.depthStencil=*/   \
                         NULL WGPU_COMMA                  /*.multisample=*/    \
                             WGPU_MULTISAMPLE_STATE_INIT                       \
                                 WGPU_COMMA /*.fragment=*/NULL WGPU_COMMA})

#define WGPU_REQUEST_ADAPTER_OPTIONS_INIT                                      \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPURequestAdapterOptions,                                             \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.compatibleSurface=*/NULL          \
             WGPU_COMMA /*.powerPreference=*/WGPUPowerPreference_Undefined     \
                 WGPU_COMMA /*.backendType=*/WGPUBackendType_Undefined         \
                     WGPU_COMMA /*.forceFallbackAdapter=*/                     \
         false WGPU_COMMA /*.compatibilityMode=*/false WGPU_COMMA})

#define WGPU_REQUIRED_LIMITS_INIT                                              \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPURequiredLimits,                                                    \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.limits=*/WGPU_LIMITS_INIT         \
             WGPU_COMMA})

#define WGPU_SAMPLER_BINDING_LAYOUT_INIT                                       \
    WGPU_MAKE_INIT_STRUCT(WGPUSamplerBindingLayout,                            \
                          {/*.nextInChain=*/NULL WGPU_COMMA /*.type=*/         \
                               WGPUSamplerBindingType_Filtering WGPU_COMMA})

#define WGPU_SAMPLER_DESCRIPTOR_INIT                                           \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUSamplerDescriptor,                                                 \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.label=*/NULL                      \
             WGPU_COMMA /*.addressModeU=*/WGPUAddressMode_ClampToEdge          \
                 WGPU_COMMA /*.addressModeV=*/WGPUAddressMode_ClampToEdge      \
                     WGPU_COMMA /*.addressModeW=*/WGPUAddressMode_ClampToEdge  \
                         WGPU_COMMA /*.magFilter=*/WGPUFilterMode_Nearest      \
                             WGPU_COMMA /*.minFilter=*/WGPUFilterMode_Nearest  \
                                 WGPU_COMMA /*.mipmapFilter=*/                 \
                                     WGPUMipmapFilterMode_Nearest              \
                                         WGPU_COMMA        /*.lodMinClamp=*/   \
         0.0f WGPU_COMMA /*.lodMaxClamp=*/32.0f WGPU_COMMA /*.compare=*/       \
             WGPUCompareFunction_Undefined                                     \
                 WGPU_COMMA /*.maxAnisotropy=*/1 WGPU_COMMA})

#define WGPU_SHADER_MODULE_DESCRIPTOR_INIT                                     \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUShaderModuleDescriptor,                                            \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.label=*/NULL WGPU_COMMA})

#define WGPU_SHADER_SOURCE_SPIRV_INIT                                          \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUShaderSourceSPIRV,                                                 \
        {/*.chain= WGPUChainedStruct*/ {                                       \
             /*.next=*/NULL WGPU_COMMA /*.sType=*/WGPUSType_ShaderSourceSPIRV  \
                 WGPU_COMMA} WGPU_COMMA /*.codeSize=*/{},                      \
         .code = {} WGPU_COMMA})

#define WGPU_SHADER_SOURCE_WGSL_INIT                                           \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUShaderSourceWGSL,                                                  \
        {/*.chain= WGPUChainedStruct*/ {                                       \
            /*.next=*/NULL WGPU_COMMA /*.sType=*/WGPUSType_ShaderSourceWGSL    \
                WGPU_COMMA} WGPU_COMMA /*.code=*/NULL WGPU_COMMA})

#define WGPU_STENCIL_FACE_STATE_INIT                                           \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUStencilFaceState,                                                  \
        {/*.compare=*/WGPUCompareFunction_Always WGPU_COMMA /*.failOp=*/       \
             WGPUStencilOperation_Keep                                         \
                 WGPU_COMMA /*.depthFailOp=*/WGPUStencilOperation_Keep         \
                     WGPU_COMMA /*.passOp=*/WGPUStencilOperation_Keep          \
                         WGPU_COMMA})

#define WGPU_STORAGE_TEXTURE_BINDING_LAYOUT_INIT                               \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUStorageTextureBindingLayout,                                       \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.access=*/                         \
             WGPUStorageTextureAccess_WriteOnly                                \
                 WGPU_COMMA /*.format=*/WGPUTextureFormat_Undefined            \
                     WGPU_COMMA /*.viewDimension=*/WGPUTextureViewDimension_2D \
                         WGPU_COMMA})

#define WGPU_SUPPORTED_LIMITS_INIT                                             \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUSupportedLimits,                                                   \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.limits=*/WGPU_LIMITS_INIT         \
             WGPU_COMMA})

#define WGPU_SURFACE_CAPABILITIES_INIT                                         \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUSurfaceCapabilities,                                               \
        {/*.nextInChain=*/NULL WGPU_COMMA              /*.formatCount=*/       \
         {} WGPU_COMMA /*.formats=*/{} WGPU_COMMA      /*.presentModeCount=*/  \
         {} WGPU_COMMA /*.presentModes=*/{} WGPU_COMMA /*.alphaModeCount=*/    \
         {} WGPU_COMMA /*.alphaModes=*/{} WGPU_COMMA})

#define WGPU_SURFACE_CONFIGURATION_INIT                                        \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUSurfaceConfiguration,                                              \
        {/*.nextInChain=*/NULL                                                 \
             WGPU_COMMA /*.device=*/{} WGPU_COMMA /*.format=*/                 \
         {} WGPU_COMMA /*.usage=*/WGPUTextureUsage_RenderAttachment            \
             WGPU_COMMA /*.viewFormatCount=*/0 WGPU_COMMA /*.viewFormats=*/    \
                 NULL WGPU_COMMA /*.alphaMode=*/WGPUCompositeAlphaMode_Auto    \
                     WGPU_COMMA /*.width=*/{} WGPU_COMMA /*.height=*/          \
         {} WGPU_COMMA /*.presentMode=*/WGPUPresentMode_Fifo WGPU_COMMA})

#define WGPU_SURFACE_DESCRIPTOR_INIT                                           \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUSurfaceDescriptor,                                                 \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.label=*/NULL WGPU_COMMA})

#define WGPU_SURFACE_TEXTURE_INIT                                              \
    WGPU_MAKE_INIT_STRUCT(WGPUSurfaceTexture,                                  \
                          {/*.texture=*/{} WGPU_COMMA /*.suboptimal=*/         \
                           {} WGPU_COMMA /*.status=*/{} WGPU_COMMA})

#define WGPU_TEXTURE_BINDING_LAYOUT_INIT                                       \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUTextureBindingLayout,                                              \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.sampleType=*/                     \
             WGPUTextureSampleType_Float                                       \
                 WGPU_COMMA /*.viewDimension=*/WGPUTextureViewDimension_2D     \
                     WGPU_COMMA /*.multisampled=*/false WGPU_COMMA})

#define WGPU_TEXTURE_BINDING_VIEW_DIMENSION_DESCRIPTOR_INIT                    \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUTextureBindingViewDimensionDescriptor,                             \
        {/*.chain= WGPUChainedStruct*/ {                                       \
            /*.next=*/NULL WGPU_COMMA /*.sType=*/                              \
                WGPUSType_TextureBindingViewDimensionDescriptor                \
                    WGPU_COMMA} WGPU_COMMA /*.textureBindingViewDimension=*/   \
             WGPUTextureViewDimension_Undefined WGPU_COMMA})

#define WGPU_TEXTURE_DATA_LAYOUT_INIT                                          \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUTextureDataLayout,                                                 \
        {/*.nextInChain=*/NULL                                                 \
             WGPU_COMMA /*.offset=*/0 WGPU_COMMA       /*.bytesPerRow=*/       \
                 WGPU_COPY_STRIDE_UNDEFINED WGPU_COMMA /*.rowsPerImage=*/      \
                     WGPU_COPY_STRIDE_UNDEFINED WGPU_COMMA})

#define WGPU_TEXTURE_DESCRIPTOR_INIT                                           \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUTextureDescriptor,                                                 \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.label=*/NULL                      \
             WGPU_COMMA /*.usage=*/{} WGPU_COMMA       /*.dimension=*/         \
                 WGPUTextureDimension_2D WGPU_COMMA    /*.size=*/              \
                     WGPU_EXTENT_3D_INIT WGPU_COMMA    /*.format=*/            \
         {} WGPU_COMMA /*.mipLevelCount=*/1 WGPU_COMMA /*.sampleCount=*/       \
         1 WGPU_COMMA /*.viewFormatCount=*/0 WGPU_COMMA /*.viewFormats=*/NULL  \
             WGPU_COMMA})

#define WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT                                      \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUTextureViewDescriptor,                                             \
        {/*.nextInChain=*/NULL WGPU_COMMA /*.label=*/NULL                      \
             WGPU_COMMA /*.format=*/WGPUTextureFormat_Undefined                \
                 WGPU_COMMA /*.dimension=*/WGPUTextureViewDimension_Undefined  \
                     WGPU_COMMA /*.baseMipLevel=*/                             \
         0 WGPU_COMMA /*.mipLevelCount=*/WGPU_MIP_LEVEL_COUNT_UNDEFINED        \
             WGPU_COMMA /*.baseArrayLayer=*/0 WGPU_COMMA /*.arrayLayerCount=*/ \
                 WGPU_ARRAY_LAYER_COUNT_UNDEFINED                              \
                     WGPU_COMMA /*.aspect=*/WGPUTextureAspect_All WGPU_COMMA})

#define WGPU_VERTEX_ATTRIBUTE_INIT                                             \
    WGPU_MAKE_INIT_STRUCT(WGPUVertexAttribute,                                 \
                          {/*.format=*/{} WGPU_COMMA /*.offset=*/              \
                           {} WGPU_COMMA /*.shaderLocation=*/{} WGPU_COMMA})

#define WGPU_VERTEX_BUFFER_LAYOUT_INIT                                         \
    WGPU_MAKE_INIT_STRUCT(WGPUVertexBufferLayout,                              \
                          {/*.arrayStride=*/{} WGPU_COMMA /*.stepMode=*/       \
                           {} WGPU_COMMA                  /*.attributeCount=*/ \
                           {} WGPU_COMMA /*.attributes=*/{} WGPU_COMMA})

#define WGPU_VERTEX_STATE_INIT                                                 \
    WGPU_MAKE_INIT_STRUCT(                                                     \
        WGPUVertexState,                                                       \
        {/*.nextInChain=*/NULL                                                 \
             WGPU_COMMA /*.module=*/{} WGPU_COMMA /*.entryPoint=*/NULL         \
                 WGPU_COMMA /*.constantCount=*/0 WGPU_COMMA /*.constants=*/    \
         {} WGPU_COMMA /*.bufferCount=*/0 WGPU_COMMA        /*.buffers=*/      \
         {} WGPU_COMMA})

#define WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT                                \
    WGPU_MAKE_INIT_STRUCT(WGPUQueueWorkDoneCallbackInfo,                       \
                          {/*.nextInChain=*/NULL WGPU_COMMA /*.callback=*/NULL \
                               WGPU_COMMA /*.userdata1=*/NULL                  \
                                   WGPU_COMMA /*.userdata2=*/NULL WGPU_COMMA})

#define WAGYU_STRING_VIEW(s)                                                   \
    WagyuStringView { s, (s != NULL) ? strlen(s) : 0 }

#endif // WEBGPU_COMPAT_H
