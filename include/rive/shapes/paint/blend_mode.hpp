#ifndef _RIVE_BLEND_MODE_HPP_
#define _RIVE_BLEND_MODE_HPP_
namespace rive
{
enum class BlendMode : unsigned char
{
    srcOver = 3,
    // Additive ("plus"): src + dst. Unlike the other modes this one is
    // parameterized -- see Drawable::additiveAmount() /
    // ShapePaint::additiveAmount() -- so it can be mixed back toward srcOver.
    // The value matches Flutter's ui.BlendMode.plus index, which is what the
    // canvas-backed renderers decode.
    additive = 12,
    screen = 14,
    overlay = 15,
    darken = 16,
    lighten = 17,
    colorDodge = 18,
    colorBurn = 19,
    hardLight = 20,
    softLight = 21,
    difference = 22,
    exclusion = 23,
    multiply = 24,
    hue = 25,
    saturation = 26,
    color = 27,
    luminosity = 28
};

constexpr uint32_t BLEND_MODE_BIT_COUNT = 5;

/// The additive amount is authored as a byte but RenderPaint::additiveness
/// takes 0-1, and every other mode ignores it. One place so the scene graph
/// and the change callbacks can never drift apart.
inline float additivenessFor(BlendMode mode, uint8_t additiveAmount)
{
    return mode == BlendMode::additive ? additiveAmount / 255.0f : 0.0f;
}
} // namespace rive
#endif
