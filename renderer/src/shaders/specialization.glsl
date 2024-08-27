layout(constant_id = CLIPPING_SPECIALIZATION_IDX) const bool kEnableClipping = false;
layout(constant_id = CLIP_RECT_SPECIALIZATION_IDX) const bool kEnableClipRect = false;
layout(constant_id = ADVANCED_BLEND_SPECIALIZATION_IDX) const bool kEnableAdvancedBlend = false;
layout(constant_id = EVEN_ODD_SPECIALIZATION_IDX) const bool kEnableEvenOdd = false;
layout(constant_id = NESTED_CLIPPING_SPECIALIZATION_IDX) const bool kEnableNestedClipping = false;
layout(constant_id = HSL_BLEND_MODES_SPECIALIZATION_IDX) const bool kEnableHSLBlendModes = false;

#define @ENABLE_CLIPPING kEnableClipping
#define @ENABLE_CLIP_RECT kEnableClipRect
#define @ENABLE_ADVANCED_BLEND kEnableAdvancedBlend
#define @ENABLE_EVEN_ODD kEnableEvenOdd
#define @ENABLE_NESTED_CLIPPING kEnableNestedClipping
#define @ENABLE_HSL_BLEND_MODES kEnableHSLBlendModes
