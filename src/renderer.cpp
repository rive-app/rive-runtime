#include "rive/math/mat2d.hpp"
#include "rive/renderer.hpp"

using namespace rive;

Mat2D rive::computeAlignment(Fit fit,
                             Alignment alignment,
                             const AABB& frame,
                             const AABB& content,
                             const float scaleFactor)
{
    float contentWidth = content.width();
    float contentHeight = content.height();
    float x = -content.left() - contentWidth * 0.5f -
              (alignment.x() * contentWidth * 0.5f);
    float y = -content.top() - contentHeight * 0.5f -
              (alignment.y() * contentHeight * 0.5f);

    float scaleX = 1.0f, scaleY = 1.0f;

    switch (fit)
    {
        case Fit::fill:
        {
            scaleX = frame.width() / contentWidth;
            scaleY = frame.height() / contentHeight;
            break;
        }
        case Fit::contain:
        {
            float minScale = std::fmin(frame.width() / contentWidth,
                                       frame.height() / contentHeight);
            scaleX = scaleY = minScale;
            break;
        }
        case Fit::cover:
        {
            float maxScale = std::fmax(frame.width() / contentWidth,
                                       frame.height() / contentHeight);
            scaleX = scaleY = maxScale;
            break;
        }
        case Fit::fitHeight:
        {
            float minScale = frame.height() / contentHeight;
            scaleX = scaleY = minScale;
            break;
        }
        case Fit::fitWidth:
        {
            float minScale = frame.width() / contentWidth;
            scaleX = scaleY = minScale;
            break;
        }
        case Fit::layout:
        {
            scaleX = scaleY = scaleFactor;
            break;
        }
        case Fit::none:
        {
            scaleX = scaleY = 1.0f;
            break;
        }
        case Fit::scaleDown:
        {
            float minScale = std::fmin(frame.width() / contentWidth,
                                       frame.height() / contentHeight);
            scaleX = scaleY = minScale < 1.0f ? minScale : 1.0f;
            break;
        }
    }

    Mat2D translation;
    translation[4] = frame.left() + frame.width() * 0.5f +
                     (alignment.x() * frame.width() * 0.5f);
    translation[5] = frame.top() + frame.height() * 0.5f +
                     (alignment.y() * frame.height() * 0.5f);

    return translation * Mat2D::fromScale(scaleX, scaleY) *
           Mat2D::fromTranslate(x, y);
}

void Renderer::translate(float tx, float ty)
{
    this->transform(Mat2D(1, 0, 0, 1, tx, ty));
}

void Renderer::scale(float sx, float sy)
{
    this->transform(Mat2D(sx, 0, 0, sy, 0, 0));
}

void Renderer::rotate(float radians)
{
    const float s = std::sin(radians);
    const float c = std::cos(radians);
    this->transform(Mat2D(c, s, -s, c, 0, 0));
}

RenderBuffer::RenderBuffer(RenderBufferType type,
                           RenderBufferFlags flags,
                           size_t sizeInBytes) :
    m_type(type), m_flags(flags), m_sizeInBytes(sizeInBytes)
{}

RenderBuffer::~RenderBuffer() {}

void* RenderBuffer::map()
{
    assert(m_mapCount == 0 ||
           !enums::is_flag_set(m_flags,
                               RenderBufferFlags::mappedOnceAtInitialization));
    assert(m_mapCount == m_unmapCount);
    RIVE_DEBUG_CODE(++m_mapCount;)
    m_dirty = true;
    return onMap();
}

void RenderBuffer::unmap()
{
    assert(m_unmapCount + 1 == m_mapCount);
    RIVE_DEBUG_CODE(++m_unmapCount;)
    onUnmap();
}

ImageMeshInstances::ImageMeshInstances(size_t count) : m_instanceData(count) {}

ImageMeshInstances::~ImageMeshInstances() {}

Span<ImageMeshInstanceData> ImageMeshInstances::edit()
{
    assert(m_editCount == m_endEditCount);
    RIVE_DEBUG_CODE(++m_editCount;)
    return m_instanceData;
}

Span<ImageMeshInstanceData> ImageMeshInstances::edit(size_t count)
{
    m_instanceData.resize(count);
    return edit();
}

void ImageMeshInstances::endEdit()
{
    assert(m_endEditCount + 1 == m_editCount);
    RIVE_DEBUG_CODE(++m_endEditCount;)
    onEndEdit();
}

void Renderer::drawImageMeshInstanced(const RenderImage* image,
                                      ImageSampler sampler,
                                      rcp<RenderBuffer> vertices_f32,
                                      rcp<RenderBuffer> uvCoords_f32,
                                      rcp<RenderBuffer> indices_u16,
                                      uint32_t vertexCount,
                                      uint32_t indexCount,
                                      rcp<ImageMeshInstances> instances)
{
    if (instances == nullptr)
    {
        return;
    }

    // The default implementation just delegates to drawImageMesh.
    // TODO(ben) Add UV transform support to drawImageMesh
    for (const ImageMeshInstanceData& instance : instances->instanceData())
    {
        save();
        transform(instance.transform);
        drawImageMesh(image,
                      sampler,
                      vertices_f32,
                      uvCoords_f32,
                      indices_u16,
                      vertexCount,
                      indexCount,
                      BlendMode::srcOver,
                      instance.opacity,
                      instance.additiveness);
        restore();
    }
}

RenderShader::RenderShader() {}
RenderShader::~RenderShader() {}

RenderPaint::RenderPaint() {}
RenderPaint::~RenderPaint() {}

RenderImage::RenderImage(const Mat2D& uvTransform) : m_uvTransform(uvTransform)
{}
RenderImage::RenderImage() {}
RenderImage::~RenderImage() {}

RenderPath::RenderPath() {}
RenderPath::~RenderPath() {}

void RenderPath::addUntrustedRawPath(const RawPath& path)
{
    RawPath sanitized(path);
    sanitized.pruneEmptySegments();
    addRawPath(sanitized);
}
