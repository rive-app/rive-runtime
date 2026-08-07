layout(constant_id = CLIPPING_SPECIALIZATION_IDX) const
    bool EnableClipping = true;
layout(constant_id = CLIP_RECT_SPECIALIZATION_IDX) const
    bool EnableClipRect = true;
layout(constant_id = ADVANCED_BLEND_SPECIALIZATION_IDX) const
    bool EnableAdvancedBlend = true;
layout(constant_id = FEATHER_SPECIALIZATION_IDX) const
    bool EnableFeather = true;
layout(constant_id = EVEN_ODD_SPECIALIZATION_IDX) const
    bool EnableEvenOdd = true;
layout(constant_id = NESTED_CLIPPING_SPECIALIZATION_IDX) const
    bool EnableNestedClipping = true;
layout(constant_id = HSL_BLEND_MODES_SPECIALIZATION_IDX) const
    bool EnableHSLBlendModes = true;
layout(constant_id = DITHER_SPECIALIZATION_IDX) const bool EnableDither = true;
layout(constant_id = MODULATED_IMAGE_SPECIALIZATION_IDX) const
    bool EnableModulatedImage = true;
layout(constant_id = CLOCKWISE_FILL_SPECIALIZATION_IDX) const
    bool ClockwiseFill = true;
layout(constant_id = NESTED_CLIP_UPDATE_ONLY_SPECIALIZATION_IDX) const
    bool NestedClipUpdateOnly = false;
layout(constant_id = BORROWED_COVERAGE_PASS_SPECIALIZATION_IDX) const
    bool BorrowedCoveragePrepass = false;
layout(constant_id = EMULATE_DYNAMIC_COLOR_WRITE_DISABLE_SPECIALIZATION_IDX)
    const bool EmulateDynamicColorWriteDisable = false;
layout(constant_id = STORE_COLOR_CLEAR_SPECIALIZATION_IDX) const
    bool StoreColorClear = false;
layout(constant_id = LOAD_COLOR_FROM_DST_TEXTURE_SPECIALIZATION_IDX) const
    bool LoadColorFromDstTexture = false;
layout(constant_id = VULKAN_VENDOR_ARM_SPECIALIZATION_IDX) const
    bool VulkanVendorARM = false;

#define @ENABLE_CLIPPING EnableClipping
#define @ENABLE_CLIP_RECT EnableClipRect
#define @ENABLE_ADVANCED_BLEND EnableAdvancedBlend
#define @DISABLE_ADVANCED_BLEND DisableAdvancedBlend
#define @ENABLE_FEATHER EnableFeather
#define @ENABLE_EVEN_ODD EnableEvenOdd
#define @ENABLE_NESTED_CLIPPING EnableNestedClipping
#define @ENABLE_HSL_BLEND_MODES EnableHSLBlendModes
#define @ENABLE_DITHER EnableDither
#define @ENABLE_MODULATED_IMAGE EnableModulatedImage
#define @CLOCKWISE_FILL ClockwiseFill
#define @NESTED_CLIP_UPDATE_ONLY NestedClipUpdateOnly
#define @BORROWED_COVERAGE_PASS BorrowedCoveragePrepass
#define @STORE_COLOR_CLEAR StoreColorClear
#define @LOAD_COLOR_FROM_DST_TEXTURE LoadColorFromDstTexture
#define @VULKAN_VENDOR_ARM VulkanVendorARM

// WebGPU has no concept of dynamic state, so we don't use the dynamic rendering
// drawTypes there, and there is no missing dynamic state to emulate.
// Furthermore, this feature gets emulated via push constant, for which
// naga/WGSL have no equivalent.
#ifndef @TARGET_WGSL
// Since SPIR-V can't omit declarations via specialization constants, only
// define @EMULATE_DYNAMIC_COLOR_WRITE_DISABLE where it is used (i.e., MSAA).
#if defined(@RENDER_MODE_MSAA)
#define @EMULATE_DYNAMIC_COLOR_WRITE_DISABLE EmulateDynamicColorWriteDisable
#endif
#endif
