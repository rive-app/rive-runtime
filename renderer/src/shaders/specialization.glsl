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
layout(constant_id = CLOCKWISE_FILL_SPECIALIZATION_IDX) const
    bool ClockwiseFill = true;
layout(constant_id = BORROWED_COVERAGE_PASS_SPECIALIZATION_IDX) const
    bool BorrowedCoveragePrepass = false;
layout(constant_id = NESTED_CLIP_UPDATE_ONLY_IDX) const
    bool NestedClipUpdateOnly = false;
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
#define @CLOCKWISE_FILL ClockwiseFill
#define @BORROWED_COVERAGE_PASS BorrowedCoveragePrepass
#define @NESTED_CLIP_UPDATE_ONLY NestedClipUpdateOnly
#define @VULKAN_VENDOR_ARM VulkanVendorARM
