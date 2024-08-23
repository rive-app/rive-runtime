/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_RENDERER_HPP_
#define _RIVE_RENDERER_HPP_

#include "rive/enum_bitset.hpp"
#include "rive/shapes/paint/color.hpp"
#include "rive/command_path.hpp"
#include "rive/layout.hpp"
#include "rive/refcnt.hpp"
#include "rive/math/aabb.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/shapes/paint/blend_mode.hpp"
#include "rive/shapes/paint/stroke_cap.hpp"
#include "rive/shapes/paint/stroke_join.hpp"
#include "utils/lite_rtti.hpp"

#include <stdio.h>
#include <cstdint>

namespace rive
{
class Vec2D;

// Helper that computes a matrix to "align" content (source) to fit inside frame (destination).
Mat2D computeAlignment(Fit, Alignment, const AABB& frame, const AABB& content);

enum class RenderBufferType
{
    index,
    vertex,
};

enum class RenderBufferFlags
{
    none = 0,
    mappedOnceAtInitialization = 1 << 0, // The client will map the buffer exactly one time, before
                                         // rendering, and will never update it again.
};
RIVE_MAKE_ENUM_BITSET(RenderBufferFlags)

class RenderBuffer : public RefCnt<RenderBuffer>, public enable_lite_rtti<RenderBuffer>
{
public:
    RenderBuffer(RenderBufferType, RenderBufferFlags, size_t sizeInBytes);
    virtual ~RenderBuffer();

    RenderBufferType type() const { return m_type; }
    RenderBufferFlags flags() const { return m_flags; }
    size_t sizeInBytes() const { return m_sizeInBytes; }

    void* map();
    void unmap();

protected:
    virtual void* onMap() = 0;
    virtual void onUnmap() = 0;

private:
    const RenderBufferType m_type;
    const RenderBufferFlags m_flags;
    const size_t m_sizeInBytes;
    RIVE_DEBUG_CODE(size_t m_mapCount = 0;)
    RIVE_DEBUG_CODE(size_t m_unmapCount = 0;)
};

enum class RenderPaintStyle
{
    stroke,
    fill
};

/*
 *  Base class for Render objects that specify the src colors.
 *
 *  Shaders are immutable, and sharable between multiple paints, etc.
 *
 *  It is common that a shader may be created with a 'localMatrix'. If this is
 *  not null, then it is applied to the shader's domain before the Renderer's CTM.
 */
class RenderShader : public RefCnt<RenderShader>, public enable_lite_rtti<RenderShader>
{
public:
    RenderShader();
    virtual ~RenderShader();
};

class RenderPaint : public RefCnt<RenderPaint>, public enable_lite_rtti<RenderPaint>
{
public:
    RenderPaint();
    virtual ~RenderPaint();

    virtual void style(RenderPaintStyle style) = 0;
    virtual void color(ColorInt value) = 0;
    virtual void thickness(float value) = 0;
    virtual void join(StrokeJoin value) = 0;
    virtual void cap(StrokeCap value) = 0;
    virtual void blendMode(BlendMode value) = 0;
    virtual void shader(rcp<RenderShader>) = 0;
    virtual void invalidateStroke() = 0;
};

class RenderImage : public RefCnt<RenderImage>, public enable_lite_rtti<RenderImage>
{
protected:
    int m_Width = 0;
    int m_Height = 0;
    Mat2D m_uvTransform;

public:
    RenderImage();
    RenderImage(const Mat2D& uvTransform);
    virtual ~RenderImage();

    int width() const { return m_Width; }
    int height() const { return m_Height; }
    const Mat2D& uvTransform() const { return m_uvTransform; }
};

class RenderPath : public CommandPath, public enable_lite_rtti<RenderPath>
{
public:
    RenderPath();
    ~RenderPath() override;

    RenderPath* renderPath() override { return this; }
    void addPath(CommandPath* path, const Mat2D& transform) override
    {
        addRenderPath(path->renderPath(), transform);
    }

    virtual void addRenderPath(RenderPath* path, const Mat2D& transform) = 0;
};

class Renderer
{
public:
    virtual ~Renderer() {}
    virtual void save() = 0;
    virtual void restore() = 0;
    virtual void transform(const Mat2D& transform) = 0;
    virtual void drawPath(RenderPath* path, RenderPaint* paint) = 0;
    virtual void clipPath(RenderPath* path) = 0;
    virtual void drawImage(const RenderImage*, BlendMode, float opacity) = 0;
    virtual void drawImageMesh(const RenderImage*,
                               rcp<RenderBuffer> vertices_f32,
                               rcp<RenderBuffer> uvCoords_f32,
                               rcp<RenderBuffer> indices_u16,
                               uint32_t vertexCount,
                               uint32_t indexCount,
                               BlendMode,
                               float opacity) = 0;

    // helpers

    void translate(float x, float y);
    void scale(float sx, float sy);
    void rotate(float radians);

    void align(Fit fit, Alignment alignment, const AABB& frame, const AABB& content)
    {
        transform(computeAlignment(fit, alignment, frame, content));
    }
};
} // namespace rive
#endif
