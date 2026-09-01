#pragma once

#include <array>
#include "generated/shaders/tessellate.glsl.exports.h"

namespace rive::gpu
{
static constexpr auto ImageDrawInstanceBaseAttributes = std::array{
    VertexAttribute{
        // m_viewMatrix (2x2)
        VertexElementFormat::float4,
        ImageDrawInstanceBase::FirstAttribIdx + 0,
        0,
        GLSL_a_imageDrawViewMatrix,
    },
    VertexAttribute{
        // m_clipRectInverseMatrix (2x2)
        VertexElementFormat::float4,
        ImageDrawInstanceBase::FirstAttribIdx + 1,
        4 * sizeof(float),
        GLSL_a_imageDrawClipRectInverseMatrix,
    },
    VertexAttribute{
        // packed: m_translate (2), m_clipRectInverseTranslate(2)
        VertexElementFormat::float4,
        ImageDrawInstanceBase::FirstAttribIdx + 2,
        8 * sizeof(float),
        GLSL_a_imageDrawTranslates,
    },
    VertexAttribute{
        // m_opacity/m_modulatedColor
        VertexElementFormat::float1,
        ImageDrawInstanceBase::FirstAttribIdx + 3,
        12 * sizeof(float),
        GLSL_a_imageDrawOpacity,
    },
    VertexAttribute{
        // m_clipID
        VertexElementFormat::uint32,
        ImageDrawInstanceBase::FirstAttribIdx + 4,
        13 * sizeof(float),
        GLSL_a_imageDrawClipID,
    },
    VertexAttribute{
        // m_blendMode
        VertexElementFormat::uint32,
        ImageDrawInstanceBase::FirstAttribIdx + 5,
        14 * sizeof(float),
        GLSL_a_imageDrawBlendMode,
    },
    VertexAttribute{
        // m_zIndex
        VertexElementFormat::uint32,
        ImageDrawInstanceBase::FirstAttribIdx + 6,
        15 * sizeof(float),
        GLSL_a_imageDrawZIndex,
    },
};

static_assert(std::size(ImageDrawInstanceBaseAttributes) ==
              ImageDrawInstanceBase::AttributeCount);
static_assert(std::size(ImageDrawInstanceBaseAttributes) ==
              IMAGE_COMMON_ATTRIB_COUNT);

// No additional attributes yet.
static constexpr auto ImageRectInstanceAttributes =
    ImageDrawInstanceBaseAttributes;

static_assert(std::size(ImageRectInstanceAttributes) ==
              IMAGE_RECT_ATTRIB_COUNT);

// No additional attributes yet.
static constexpr auto ImageMeshInstanceAttributes =
    ImageDrawInstanceBaseAttributes;

static_assert(std::size(ImageMeshInstanceAttributes) ==
              IMAGE_MESH_ATTRIB_COUNT);

} // namespace rive::gpu