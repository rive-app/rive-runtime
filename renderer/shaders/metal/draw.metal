#define VERTEX
#define FRAGMENT
#
#include <metal_stdlib>

#include "../../../out/obj/generated/metal.minified.glsl"
#include "../../../out/obj/generated/common.minified.glsl"

#define ENABLE_ADVANCED_BLEND
#include "../../../out/obj/generated/advanced_blend.minified.glsl"
#define ENABLE_HSL_BLEND_MODES
#include "../../../out/obj/generated/advanced_blend.minified.glsl"
#undef ENABLE_HSL_BLEND_MODES
#undef ENABLE_ADVANCED_BLEND

// Compile all combinations of flags into a single compilation unit.
// Each combination gets added to a namespace whose name has the following bits set for each flag:
//     ENABLE_ADVANCED_BLEND:   r0001
//     ENABLE_PATH_CLIPPING:    r0010
//     ENABLE_EVEN_ODD:         r0100
//     ENABLE_HSL_BLEND_MODES:  r1000

// 4-bit "Gray Code": enumerate all bit combinations by only modifying one single bit each step.
//     0000
//     0001
//     0011
//     0010
//     0110
//     0111
//     0101
//     0100
//     1100
//     1101
//     1111
//     1110
//     1010
//     1011
//     1001
//     1000

// Vertex and fragment combinations.
namespace r0000
{
#include "../../../out/obj/generated/draw.minified.glsl"
}

#define ENABLE_ADVANCED_BLEND
namespace r0001
{
#include "../../../out/obj/generated/draw.minified.glsl"
}

#define ENABLE_PATH_CLIPPING
namespace r0011
{
#include "../../../out/obj/generated/draw.minified.glsl"
}

#undef ENABLE_ADVANCED_BLEND
namespace r0010
{
#include "../../../out/obj/generated/draw.minified.glsl"
}

// Fragment-only combinations.
#undef VERTEX
#define ENABLE_EVEN_ODD
namespace r0110
{
#include "../../../out/obj/generated/draw.minified.glsl"
}

#define ENABLE_ADVANCED_BLEND
namespace r0111
{
#include "../../../out/obj/generated/draw.minified.glsl"
}

#undef ENABLE_PATH_CLIPPING
namespace r0101
{
#include "../../../out/obj/generated/draw.minified.glsl"
}

#undef ENABLE_ADVANCED_BLEND
namespace r0100
{
#include "../../../out/obj/generated/draw.minified.glsl"
}

#define ENABLE_HSL_BLEND_MODES
namespace r1100
{
// HSL blend without advanced blend doesn't happen.
}

#define ENABLE_ADVANCED_BLEND
namespace r1101
{
#include "../../../out/obj/generated/draw.minified.glsl"
}

#define ENABLE_PATH_CLIPPING
namespace r1111
{
#include "../../../out/obj/generated/draw.minified.glsl"
}

#undef ENABLE_ADVANCED_BLEND
namespace r1110
{
// HSL blend without advanced blend doesn't happen.
}

#undef ENABLE_EVEN_ODD
namespace r1010
{
// HSL blend without advanced blend doesn't happen.
}

#define ENABLE_ADVANCED_BLEND
namespace r1011
{
#include "../../../out/obj/generated/draw.minified.glsl"
}

#undef ENABLE_PATH_CLIPPING
namespace r1001
{
#include "../../../out/obj/generated/draw.minified.glsl"
}

#undef ENABLE_ADVANCED_BLEND
namespace r1000
{
// HSL blend without advanced blend doesn't happen.
}
