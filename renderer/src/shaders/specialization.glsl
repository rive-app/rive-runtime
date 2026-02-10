#ifndef @SPEC_CONST_NONE

layout(constant_id = CLIPPING_SPECIALIZATION_IDX) const
    bool kEnableClipping = true;
layout(constant_id = CLIP_RECT_SPECIALIZATION_IDX) const
    bool kEnableClipRect = true;
layout(constant_id = ADVANCED_BLEND_SPECIALIZATION_IDX) const
    bool kEnableAdvancedBlend = true;
layout(constant_id = FEATHER_SPECIALIZATION_IDX) const
    bool kEnableFeather = true;
layout(constant_id = EVEN_ODD_SPECIALIZATION_IDX) const
    bool kEnableEvenOdd = true;
layout(constant_id = NESTED_CLIPPING_SPECIALIZATION_IDX) const
    bool kEnableNestedClipping = true;
layout(constant_id = HSL_BLEND_MODES_SPECIALIZATION_IDX) const
    bool kEnableHSLBlendModes = true;
layout(constant_id = DITHER_SPECIALIZATION_IDX) const bool kEnableDither = true;
layout(constant_id = CLOCKWISE_FILL_SPECIALIZATION_IDX) const
    bool kClockwiseFill = true;
layout(constant_id = BORROWED_COVERAGE_PASS_SPECIALIZATION_IDX) const
    bool kBorrowedCoveragePrepass = true;
layout(constant_id = VULKAN_VENDOR_ID_SPECIALIZATION_IDX) const uint
    kVulkanVendorID = 0;

#define @ENABLE_CLIPPING kEnableClipping
#define @ENABLE_CLIP_RECT kEnableClipRect
#define @ENABLE_ADVANCED_BLEND kEnableAdvancedBlend
#define @ENABLE_FEATHER kEnableFeather
#define @ENABLE_EVEN_ODD kEnableEvenOdd
#define @ENABLE_NESTED_CLIPPING kEnableNestedClipping
#define @ENABLE_HSL_BLEND_MODES kEnableHSLBlendModes
#define @ENABLE_DITHER kEnableDither
#define @CLOCKWISE_FILL kClockwiseFill
#define @BORROWED_COVERAGE_PASS kBorrowedCoveragePrepass
#define @VULKAN_VENDOR_ID kVulkanVendorID

#else

// Specialization constants aren't supported; just compile an ubershader.
#define @ENABLE_CLIPPING true
#define @ENABLE_CLIP_RECT true
#define @ENABLE_ADVANCED_BLEND true
#define @ENABLE_FEATHER true
#define @ENABLE_EVEN_ODD true
#define @ENABLE_NESTED_CLIPPING true
#define @ENABLE_HSL_BLEND_MODES true
#define @ENABLE_DITHER true
#define @CLOCKWISE_FILL true
#define @BORROWED_COVERAGE_PASS true
#define @VULKAN_VENDOR_ID 0

#endif
