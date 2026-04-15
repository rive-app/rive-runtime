#ifndef DECLARE_UNIFORM_FLOAT
#define DECLARE_UNIFORM_FLOAT(UNIFORM_NAME) float UNIFORM_NAME;
#endif
#ifndef DECLARE_UNIFORM_UINT
#define DECLARE_UNIFORM_UINT(UNIFORM_NAME) uint UNIFORM_NAME;
#endif
#ifndef DECLARE_UNIFORM_INT4
#define DECLARE_UNIFORM_INT4(UNIFORM_NAME) int4 UNIFORM_NAME;
#endif
#ifndef DECLARE_UNIFORM_FLOAT2
#define DECLARE_UNIFORM_FLOAT2(UNIFORM_NAME) float2 UNIFORM_NAME;
#endif
#ifndef DECLARE_UNIFORM_FLOAT4
#define DECLARE_UNIFORM_FLOAT4(UNIFORM_NAME) float4 UNIFORM_NAME;
#endif

#ifndef FLUSH_UNIFORMS_NAME
#define FLUSH_UNIFORMS_NAME @FlushUniforms
#endif

UNIFORM_BLOCK_BEGIN(FLUSH_UNIFORM_BUFFER_IDX, FLUSH_UNIFORMS_NAME)
DECLARE_UNIFORM_FLOAT(gradInverseViewportY)
DECLARE_UNIFORM_FLOAT(tessInverseViewportY)
DECLARE_UNIFORM_FLOAT(renderTargetInverseViewportX)
DECLARE_UNIFORM_FLOAT(renderTargetInverseViewportY)
DECLARE_UNIFORM_UINT(renderTargetWidth)
DECLARE_UNIFORM_UINT(renderTargetHeight)
// Only used if clears are implemented as draws.
DECLARE_UNIFORM_UINT(colorClearValue)
// Only used if clears are implemented as draws.
DECLARE_UNIFORM_UINT(coverageClearValue)
// drawBounds, or renderTargetBounds if there is a clear. (LTRB.)
DECLARE_UNIFORM_INT4(renderTargetUpdateBounds)
// 1 / [atlasWidth, atlasHeight]
DECLARE_UNIFORM_FLOAT2(atlasTextureInverseSize)
// 2 / atlasContentBounds
DECLARE_UNIFORM_FLOAT2(atlasContentInverseViewport)
DECLARE_UNIFORM_UINT(coverageBufferPrefix)
// GLSL doesn't appear to provide a lightweight, region-local barrier for memory
// ordering outside of memoryBarrier*(), which have severe consequences for
// tiling. When we are already relying on other API level barriers and only need
// to guard against instruction reordering, we can multiply by a tiny epsilon
// instead, and introduce artifical dependencies that enforce ordering but don't
// actually have an effect on the final outcome.
DECLARE_UNIFORM_FLOAT(epsilonForPseudoMemoryBarrier)
// Spacing between adjacent path IDs (1 if IEEE compliant).
DECLARE_UNIFORM_UINT(pathIDGranularity)
DECLARE_UNIFORM_FLOAT(vertexDiscardValue)
DECLARE_UNIFORM_FLOAT(mipMapLODBias)
DECLARE_UNIFORM_UINT(maxPathId)
DECLARE_UNIFORM_FLOAT(ditherScale)
DECLARE_UNIFORM_FLOAT(ditherBias)
// Amount by which to multiply a computed dither value when storing as RGB10 (as
// opposed to writing it out to the framebuffer).
DECLARE_UNIFORM_FLOAT(ditherConversionToRGB10)
// Debugging.
DECLARE_UNIFORM_UINT(wireframeEnabled)
UNIFORM_BLOCK_END(uniforms)