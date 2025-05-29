#include "utils/serializing_factory.hpp"
#include "rive/decoders/bitmap_decoder.hpp"
#include "rive/core/binary_reader.hpp"
#include <cstring>
#include <stdlib.h>
#include <filesystem>
#include <inttypes.h>
#include <unordered_map>

using namespace rive;

// Threshold for floating point tests.
static const float epsilon = 0.001f;

enum class SerializeOp : unsigned char
{
    makeRenderBuffer = 0,
    makeLinearGradient = 1,
    makeRadialGradient = 2,
    makeRenderPath = 3,
    makeRenderPaint = 5,
    decodeImage = 6,
    save = 7,
    restore = 8,
    transform = 9,
    drawPath = 10,
    clipPath = 11,
    drawImage = 12,
    drawImageMesh = 13,

    // RenderBuffer
    setVertexBufferData = 14,
    setIndexBufferData = 15,

    // RenderPath
    addRawPath = 16,
    rewind = 17,
    fillRule = 18,

    // RenderPaint
    style = 20,
    color = 21,
    thickness = 22,
    join = 23,
    cap = 24,
    feather = 25,
    blendMode = 26,
    shader = 27,

    frame = 28,
    frameSize = 29,

};

static const char* opToName(SerializeOp op)
{
    switch (op)
    {
        case SerializeOp::makeRenderBuffer:
            return "makeRenderBuffer";
        case SerializeOp::makeLinearGradient:
            return "makeLinearGradient";
        case SerializeOp::makeRadialGradient:
            return "makeRadialGradient";
        case SerializeOp::makeRenderPath:
            return "makeRenderPath";
        case SerializeOp::makeRenderPaint:
            return "makeRenderPaint";
        case SerializeOp::decodeImage:
            return "decodeImage";
        case SerializeOp::save:
            return "save";
        case SerializeOp::restore:
            return "restore";
        case SerializeOp::transform:
            return "transform";
        case SerializeOp::drawPath:
            return "drawPath";
        case SerializeOp::clipPath:
            return "clipPath";
        case SerializeOp::drawImage:
            return "drawImage";
        case SerializeOp::drawImageMesh:
            return "drawImageMesh";

        // RenderBuffer
        case SerializeOp::setVertexBufferData:
            return "setVertexBufferData";
        case SerializeOp::setIndexBufferData:
            return "setIndexBufferData";

        // RenderPath
        case SerializeOp::addRawPath:
            return "addRawPath";
        case SerializeOp::rewind:
            return "rewind";
        case SerializeOp::fillRule:
            return "fillRule";

        // RenderPaint
        case SerializeOp::style:
            return "style";
        case SerializeOp::color:
            return "color";
        case SerializeOp::thickness:
            return "thickness";
        case SerializeOp::join:
            return "join";
        case SerializeOp::cap:
            return "cap";
        case SerializeOp::feather:
            return "feather";
        case SerializeOp::blendMode:
            return "blendMode";
        case SerializeOp::shader:
            return "shader";

        case SerializeOp::frame:
            return "frame";
        case SerializeOp::frameSize:
            return "frameSize";
    }
    return "???";
}

class SerializingRenderImage : public RenderImage
{
public:
    SerializingRenderImage(uint64_t id, uint32_t width, uint32_t height) :
        m_id(id)
    {
        m_Width = width;
        m_Height = height;
    }

    uint64_t id() const { return m_id; }

private:
    uint64_t m_id;
};

static void serializeRawPath(BinaryWriter* writer, const RawPath& path)
{
    auto verbs = path.verbs();
    auto points = path.points();
    writer->writeVarUint((uint64_t)verbs.size());
    for (auto verb : verbs)
    {
        writer->writeVarUint((uint64_t)verb);
    }
    writer->writeVarUint((uint64_t)points.size());
    for (auto point : points)
    {
        writer->writeFloat(point.x);
        writer->writeFloat(point.y);
    }
}

class SerializingRenderShader : public RenderShader
{
public:
    SerializingRenderShader(uint64_t id) : m_id(id) {}
    uint64_t id() const { return m_id; }

private:
    uint64_t m_id;
};

class SerializingRenderPaint : public RenderPaint
{
public:
    SerializingRenderPaint(BinaryWriter* writer, uint64_t id) :
        m_id(id), m_writer(writer)
    {
        m_writer->writeVarUint((uint32_t)SerializeOp::makeRenderPaint);
        m_writer->writeVarUint(m_id);
    }

    void color(unsigned int value) override
    {
        if (m_color == value)
        {
            return;
        }
        m_color = value;
        m_writer->writeVarUint((uint32_t)SerializeOp::color);
        m_writer->writeVarUint(m_id);
        m_writer->writeVarUint(m_color);
    }

    void style(RenderPaintStyle value) override
    {
        if (m_stroked == (value == RenderPaintStyle::stroke))
        {
            return;
        }
        m_stroked = value == RenderPaintStyle::stroke;
        m_writer->writeVarUint((uint32_t)SerializeOp::style);
        m_writer->writeVarUint(m_id);
        m_writer->writeVarUint((uint32_t)(m_stroked ? 0 : 1));
    }

    void thickness(float value) override
    {
        if (m_thickness == value)
        {
            return;
        }
        m_thickness = value;
        m_writer->writeVarUint((uint32_t)SerializeOp::thickness);
        m_writer->writeVarUint(m_id);
        m_writer->writeFloat(m_thickness);
    }

    void join(StrokeJoin value) override
    {
        if (m_join == value)
        {
            return;
        }
        m_join = value;
        m_writer->writeVarUint((uint32_t)SerializeOp::join);
        m_writer->writeVarUint(m_id);
        m_writer->writeVarUint((uint32_t)m_join);
    }

    void cap(StrokeCap value) override
    {
        if (m_cap == value)
        {
            return;
        }
        m_cap = value;
        m_writer->writeVarUint((uint32_t)SerializeOp::cap);
        m_writer->writeVarUint(m_id);
        m_writer->writeVarUint((uint32_t)m_cap);
    }

    void blendMode(BlendMode value) override
    {
        if (m_blendMode == value)
        {
            return;
        }
        m_blendMode = value;
        m_writer->writeVarUint((uint32_t)SerializeOp::blendMode);
        m_writer->writeVarUint(m_id);
        m_writer->writeVarUint((uint32_t)m_blendMode);
    }

    void shader(rcp<RenderShader> shader) override
    {
        if (m_shader == shader)
        {
            return;
        }
        m_shader = shader;
        m_writer->writeVarUint((uint32_t)SerializeOp::shader);
        m_writer->writeVarUint(m_id);
        m_writer->writeVarUint(
            static_cast<SerializingRenderShader*>(shader.get())->id());
    }
    void invalidateStroke() override {}
    void feather(float value) override
    {
        if (m_feather == value)
        {
            return;
        }
        m_feather = value;
        m_writer->writeVarUint((uint32_t)SerializeOp::feather);
        m_writer->writeVarUint(m_id);
        m_writer->writeFloat(m_feather);
    }

    uint64_t id() const { return m_id; }

private:
    uint64_t m_id;
    BinaryWriter* m_writer;
    rcp<RenderShader> m_shader;

    unsigned int m_color = 0xFF000000;
    float m_thickness = 1;
    StrokeJoin m_join = StrokeJoin::miter;
    StrokeCap m_cap = StrokeCap::butt;
    float m_feather = 0;
    BlendMode m_blendMode = BlendMode::srcOver;
    bool m_stroked = false;
};

class SerializingRenderPath : public RenderPath
{
public:
    void rewind() override
    {
        m_writer->writeVarUint((uint32_t)SerializeOp::rewind);
        m_writer->writeVarUint(m_id);
    }

    void fillRule(FillRule value) override
    {
        if (m_fillRule == value)
        {
            return;
        }
        m_fillRule = value;
        m_writer->writeVarUint((uint32_t)SerializeOp::fillRule);
        m_writer->writeVarUint(m_id);
        m_writer->writeVarUint((uint64_t)value);
    }

    void addPath(CommandPath* path, const Mat2D& transform) override
    {
        RIVE_UNREACHABLE();
    }
    void addRenderPath(RenderPath* path, const Mat2D& transform) override
    {
        RIVE_UNREACHABLE();
    }

    void moveTo(float x, float y) override { RIVE_UNREACHABLE(); }
    void lineTo(float x, float y) override { RIVE_UNREACHABLE(); }
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y)
        override
    {
        RIVE_UNREACHABLE();
    }
    void close() override { RIVE_UNREACHABLE(); }

    void addRawPath(const RawPath& path) override
    {
        m_rawPath.addPath(path, nullptr);

        m_writer->writeVarUint((uint32_t)SerializeOp::addRawPath);
        m_writer->writeVarUint(m_id);
        serializeRawPath(m_writer, path);
    }

    SerializingRenderPath(BinaryWriter* writer, uint64_t id) :
        m_id(id), m_writer(writer)
    {
        m_writer->writeVarUint((uint32_t)SerializeOp::makeRenderPath);
        m_writer->writeVarUint(m_id);
    }

    SerializingRenderPath(BinaryWriter* writer,
                          uint64_t id,
                          const RawPath& path,
                          FillRule fillRule) :
        m_id(id), m_fillRule(fillRule), m_rawPath(path), m_writer(writer)
    {
        m_writer->writeVarUint((uint32_t)SerializeOp::makeRenderPath);
        m_writer->writeVarUint(m_id);
        this->fillRule(fillRule);
        addRawPath(path);
    }

    uint64_t id() const { return m_id; }

private:
    uint64_t m_id;
    FillRule m_fillRule = FillRule::nonZero;
    RawPath m_rawPath;
    BinaryWriter* m_writer;
};

class SerializingRenderBuffer : public RenderBuffer
{
public:
    SerializingRenderBuffer(BinaryWriter* writer,
                            uint64_t id,
                            RenderBufferType type,
                            RenderBufferFlags flags,
                            size_t sizeInBytes) :
        RenderBuffer(type, flags, sizeInBytes),
        m_writer(writer),
        m_bytes(sizeInBytes),
        m_id(id)
    {
        m_writer->writeVarUint((uint32_t)SerializeOp::makeRenderBuffer);
        m_writer->writeVarUint(m_id);
        m_writer->writeVarUint((uint64_t)sizeInBytes);
        m_writer->writeVarUint((uint32_t)type);
        m_writer->writeVarUint((uint32_t)flags);
    }

    uint64_t id() const { return m_id; }
    void* onMap() override { return m_bytes.data(); }
    void onUnmap() override
    {
        switch (type())
        {
            case RenderBufferType::index:
                m_writer->writeVarUint(
                    (uint32_t)SerializeOp::setIndexBufferData);
                break;
            case RenderBufferType::vertex:
                m_writer->writeVarUint(
                    (uint32_t)SerializeOp::setVertexBufferData);
                break;
            default:
                RIVE_UNREACHABLE();
        }
        m_writer->writeVarUint(m_id);
        switch (type())
        {
            case RenderBufferType::vertex:
            {
                auto count = m_bytes.size() / sizeof(float);
                auto floatBuffer = reinterpret_cast<float*>(m_bytes.data());
                for (size_t i = 0; i < count; i++)
                {
                    m_writer->writeFloat(floatBuffer[i]);
                }
                break;
            }
            case RenderBufferType::index:
            {
                auto count = m_bytes.size() / sizeof(uint16_t);
                auto shortBuffer = reinterpret_cast<uint16_t*>(m_bytes.data());
                for (size_t i = 0; i < count; i++)
                {
                    m_writer->writeVarUint((uint32_t)shortBuffer[i]);
                }
                break;
            }
        }
    }

private:
    BinaryWriter* m_writer;
    std::vector<uint8_t> m_bytes;
    uint64_t m_id;
};

rcp<RenderBuffer> SerializingFactory::makeRenderBuffer(RenderBufferType type,
                                                       RenderBufferFlags flags,
                                                       size_t size)
{
    return make_rcp<SerializingRenderBuffer>(&m_writer,
                                             m_renderBufferId++,
                                             type,
                                             flags,
                                             size);
}

rcp<RenderShader> SerializingFactory::makeLinearGradient(
    float sx,
    float sy,
    float ex,
    float ey,
    const ColorInt colors[], // [count]
    const float stops[],     // [count]
    size_t count)
{
    auto id = m_renderShaderId++;
    m_writer.writeVarUint((uint32_t)SerializeOp::makeLinearGradient);
    m_writer.writeVarUint(id);
    m_writer.writeVarUint((uint64_t)count);
    for (auto i = 0; i < count; i++)
    {
        m_writer.writeVarUint(colors[i]);
        m_writer.writeFloat(stops[i]);
    }
    m_writer.writeFloat(sx);
    m_writer.writeFloat(sy);
    m_writer.writeFloat(ex);
    m_writer.writeFloat(ey);

    return make_rcp<SerializingRenderShader>(id);
}

rcp<RenderShader> SerializingFactory::makeRadialGradient(
    float cx,
    float cy,
    float radius,
    const ColorInt colors[], // [count]
    const float stops[],     // [count]
    size_t count)
{
    auto id = m_renderShaderId++;
    m_writer.writeVarUint((uint32_t)SerializeOp::makeRadialGradient);
    m_writer.writeVarUint(id);
    m_writer.writeVarUint((uint64_t)count);
    for (auto i = 0; i < count; i++)
    {
        m_writer.writeVarUint(colors[i]);
        m_writer.writeFloat(stops[i]);
    }
    m_writer.writeFloat(cx);
    m_writer.writeFloat(cy);
    m_writer.writeFloat(radius);

    return make_rcp<SerializingRenderShader>(id);
}

rcp<RenderPath> SerializingFactory::makeRenderPath(RawPath& rawPath,
                                                   FillRule fillRule)
{
    return make_rcp<SerializingRenderPath>(&m_writer,
                                           m_renderPathId++,
                                           rawPath,
                                           fillRule);
}

rcp<RenderPath> SerializingFactory::makeEmptyRenderPath()
{
    return make_rcp<SerializingRenderPath>(&m_writer, m_renderPathId++);
}

rcp<RenderPaint> SerializingFactory::makeRenderPaint()
{
    return make_rcp<SerializingRenderPaint>(&m_writer, m_renderPaintId++);
}

rcp<RenderImage> SerializingFactory::decodeImage(Span<const uint8_t> data)
{
    auto id = m_renderImageId++;
    m_writer.writeVarUint((uint32_t)SerializeOp::decodeImage);
    m_writer.writeVarUint(id);
    m_writer.writeVarUint((uint64_t)data.size());
    m_writer.write(data.data(), data.size());

    auto bitmap = Bitmap::decode(data.data(), data.size());
    if (!bitmap)
    {
        return nullptr;
    }

    return make_rcp<SerializingRenderImage>(id,
                                            bitmap->width(),
                                            bitmap->height());
}

class SerializingRenderer : public Renderer
{
public:
    SerializingRenderer(BinaryWriter* writer) : m_writer(writer) {}

    void save() override
    {
        m_writer->writeVarUint((uint32_t)SerializeOp::save);
    }
    void restore() override
    {
        m_writer->writeVarUint((uint32_t)SerializeOp::restore);
    }
    void transform(const Mat2D& mat) override
    {
        m_writer->writeVarUint((uint32_t)SerializeOp::transform);
        for (int i = 0; i < 6; i++)
        {
            m_writer->writeFloat(mat[i]);
        }
    }

    void drawPath(RenderPath* path, RenderPaint* paint) override
    {
        m_writer->writeVarUint((uint32_t)SerializeOp::drawPath);
        m_writer->writeVarUint(static_cast<SerializingRenderPath*>(path)->id());
        m_writer->writeVarUint(
            static_cast<SerializingRenderPaint*>(paint)->id());
    }

    void clipPath(RenderPath* path) override
    {
        m_writer->writeVarUint((uint32_t)SerializeOp::clipPath);
        m_writer->writeVarUint(static_cast<SerializingRenderPath*>(path)->id());
    }

    virtual void drawImage(const RenderImage* image,
                           ImageSampler samplerOptions,
                           BlendMode blendMode,
                           float opacity) override
    {
        m_writer->writeVarUint((uint32_t)SerializeOp::drawImage);
        m_writer->writeVarUint(
            static_cast<const SerializingRenderImage*>(image)->id());
        m_writer->writeVarUint((uint32_t)blendMode);
        m_writer->writeFloat(opacity);
    }

    virtual void drawImageMesh(const RenderImage* image,
                               ImageSampler samplerOptions,
                               rcp<RenderBuffer> positions,
                               rcp<RenderBuffer> uvs,
                               rcp<RenderBuffer> indices,
                               uint32_t vertexCount,
                               uint32_t indexCount,
                               BlendMode blendMode,
                               float opacity) override
    {
        m_writer->writeVarUint((uint32_t)SerializeOp::drawImageMesh);
        m_writer->writeVarUint(
            static_cast<const SerializingRenderImage*>(image)->id());
        m_writer->writeVarUint((uint32_t)blendMode);
        m_writer->writeFloat(opacity);
        m_writer->writeVarUint(
            static_cast<SerializingRenderBuffer*>(positions.get())->id());
        m_writer->writeVarUint(
            static_cast<SerializingRenderBuffer*>(uvs.get())->id());
        m_writer->writeVarUint(
            static_cast<SerializingRenderBuffer*>(indices.get())->id());
    }

private:
    BinaryWriter* m_writer;
};

SerializingFactory::SerializingFactory() : m_writer(&m_buffer)
{
    m_writer.write((const uint8_t*)"SRIV", 4);
    m_writer.writeVarUint((uint32_t)1);
}

std::unique_ptr<Renderer> SerializingFactory::makeRenderer()
{
    return rivestd::make_unique<SerializingRenderer>(&m_writer);
}

void SerializingFactory::addFrame()
{
    m_writer.writeVarUint((uint32_t)SerializeOp::frame);
}

void SerializingFactory::frameSize(uint32_t width, uint32_t height)
{
    m_writer.writeVarUint((uint32_t)SerializeOp::frameSize);
    m_writer.writeVarUint(width);
    m_writer.writeVarUint(height);
}

void SerializingFactory::save(const char* filename)
{
    FILE* fp = fopen(filename, "wb");
    fwrite(m_buffer.data(), 1, m_buffer.size(), fp);
    fclose(fp);
}

void SerializingFactory::saveTarnished(const char* filename)
{
    auto path = std::string("silvers/tarnished/");
    if (!std::filesystem::exists(path))
    {
        if (!std::filesystem::create_directories(path))
        {
            fprintf(stderr, "failed to create directory %s\n", path.c_str());
        }
    }
    auto fullFileName = path + std::string(filename) + std::string(".sriv");
    save(fullFileName.c_str());
}

static bool varUintMatches(uint64_t op,
                           std::string name,
                           BinaryReader& readerA,
                           BinaryReader& readerB,
                           uint64_t* value = nullptr)
{
    auto valueA = readerA.readVarUint64();
    auto valueB = readerB.readVarUint64();
    if (valueA != valueB)
    {
        fprintf(stderr,
                "%s for %s doesn't match %" PRIu64 " != %" PRIu64 "\n",
                name.c_str(),
                opToName((SerializeOp)op),
                valueA,
                valueB);
        return false;
    }
    if (value != nullptr)
    {
        *value = valueA;
    }
    return true;
}

static bool bytesMatches(uint64_t op,
                         std::string name,
                         uint64_t size,
                         BinaryReader& readerA,
                         BinaryReader& readerB)
{
    for (int i = 0; i < size; i++)
    {
        auto valueA = readerA.readByte();
        auto valueB = readerB.readByte();
        if (valueA != valueB)
        {
            fprintf(stderr,
                    "%s [%i] for %s doesn't match %hhu != %hhu\n",
                    name.c_str(),
                    i,
                    opToName((SerializeOp)op),
                    valueA,
                    valueB);
            return false;
        }
    }
    return true;
}

static bool floatMatches(uint64_t op,
                         std::string name,
                         BinaryReader& readerA,
                         BinaryReader& readerB,
                         float* value = nullptr)
{
    auto valueA = readerA.readFloat32();
    auto valueB = readerB.readFloat32();
    if (std::abs(valueA - valueB) > epsilon)
    {
        fprintf(stderr,
                "%s for %s doesn't match %f != %f\n",
                name.c_str(),
                opToName((SerializeOp)op),
                valueA,
                valueB);
        return false;
    }
    if (value != nullptr)
    {
        *value = valueA;
    }
    return true;
}

static bool shortMatches(uint64_t op,
                         std::string name,
                         BinaryReader& readerA,
                         BinaryReader& readerB,
                         uint16_t* value = nullptr)
{
    auto valueA = readerA.readUint16();
    auto valueB = readerB.readUint16();
    if (valueA != valueB)
    {
        fprintf(stderr,
                "%s for %s doesn't match %i != %i\n",
                name.c_str(),
                opToName((SerializeOp)op),
                valueA,
                valueB);
        return false;
    }
    if (value != nullptr)
    {
        *value = valueA;
    }
    return true;
}

static bool vec2DMatches(uint64_t op,
                         std::string name,
                         BinaryReader& readerA,
                         BinaryReader& readerB,
                         Vec2D* value = nullptr)
{
    auto ax = readerA.readFloat32();
    auto ay = readerA.readFloat32();

    auto bx = readerB.readFloat32();
    auto by = readerB.readFloat32();

    // if (ax != bx || ay != by)
    if (rive::Vec2D::distance(Vec2D(ax, ay), Vec2D(bx, by)) > epsilon)
    {
        fprintf(stderr,
                "%s for %s doesn't match (%f, %f) != (%f, %f)\n",
                name.c_str(),
                opToName((SerializeOp)op),
                ax,
                ay,
                bx,
                by);
        return false;
    }
    if (value != nullptr)
    {
        *value = Vec2D(ax, ay);
    }
    return true;
}

static bool gradientMatches(uint64_t op,
                            std::string name,
                            BinaryReader& readerA,
                            BinaryReader& readerB,
                            Vec2D* value = nullptr)
{
    if (!varUintMatches(op,
                        std::string("make_") + name + std::string("_id"),
                        readerA,
                        readerB))
    {
        return false;
    }
    uint64_t count = 0;
    if (!varUintMatches(op,
                        std::string("make_") + name + std::string("_count"),
                        readerA,
                        readerB,
                        &count))
    {
        return false;
    }
    for (size_t i = 0; i < count; i++)
    {
        std::string colorName = std::string("make_") + name +
                                std::string("_color_") + std::to_string(i);
        if (!varUintMatches(op, colorName, readerA, readerB))
        {
            return false;
        }
        std::string stopName = std::string("make_") + name +
                               std::string("_stop_") + std::to_string(i);
        if (!floatMatches(op, stopName, readerA, readerB))
        {
            return false;
        }
    }
    return true;
}

bool advancedMatch(std::vector<uint8_t>& fileA, std::vector<uint8_t>& fileB)
{
    BinaryReader readerA(fileA);
    BinaryReader readerB(fileB);
    if (readerA.readByte() != 'S' || readerA.readByte() != 'R' ||
        readerA.readByte() != 'I' || readerA.readByte() != 'V')
    {
        fprintf(stderr, "advancedMatch: invalid header A\n");
        return false;
    }
    if (readerB.readByte() != 'S' || readerB.readByte() != 'R' ||
        readerB.readByte() != 'I' || readerB.readByte() != 'V')
    {
        fprintf(stderr, "advancedMatch: invalid header B\n");
        return false;
    }

    if (readerA.readVarUint64() != 1 || readerB.readVarUint64() != 1)
    {
        fprintf(stderr, "advancedMatch: invalid version\n");
        return false;
    }
    std::unordered_map<uint64_t, uint64_t> renderBufferSizes;
    while (!readerA.reachedEnd())
    {
        if (readerB.reachedEnd())
        {
            fprintf(stderr, "generated file is shorter.\n");
            return false;
        }
        auto opA = readerA.readVarUint64();
        auto opB = readerB.readVarUint64();
        if (opA != opB)
        {
            fprintf(stderr,
                    "expected %s but got %s\n",
                    opToName((SerializeOp)opA),
                    opToName((SerializeOp)opB));
        }
        switch ((SerializeOp)opA)
        {
            case SerializeOp::makeRenderBuffer:
            {
                uint64_t id = 0;
                uint64_t size = 0;
                if (!varUintMatches(opA,
                                    "make_renderbuffer_id",
                                    readerA,
                                    readerB,
                                    &id))
                {
                    return false;
                }
                if (!varUintMatches(opA,
                                    "make_renderbuffer_size",
                                    readerA,
                                    readerB,
                                    &size))
                {
                    return false;
                }
                if (!varUintMatches(opA,
                                    "make_renderbuffer_flags",
                                    readerA,
                                    readerB))
                {
                    return false;
                }
                renderBufferSizes[id] = size;
                break;
            }
            case SerializeOp::makeLinearGradient:
                if (!gradientMatches(opA, "lineargradient", readerA, readerB))
                {
                    return false;
                }
                if (!vec2DMatches(opA,
                                  "make_lineargradient_start",
                                  readerA,
                                  readerB))
                {
                    return false;
                }
                if (!vec2DMatches(opA,
                                  "make_lineargradient_end",
                                  readerA,
                                  readerB))
                {
                    return false;
                }
                break;
            case SerializeOp::makeRadialGradient:
                if (!gradientMatches(opA, "lineargradient", readerA, readerB))
                {
                    return false;
                }
                if (!vec2DMatches(opA,
                                  "make_lineargradient_center",
                                  readerA,
                                  readerB))
                {
                    return false;
                }
                if (!floatMatches(opA,
                                  "make_lineargradient_radius",
                                  readerA,
                                  readerB))
                {
                    return false;
                }
                break;
            case SerializeOp::makeRenderPath:
                if (!varUintMatches(opA,
                                    "make_renderpath_id",
                                    readerA,
                                    readerB))
                {
                    return false;
                }
                break;
            case SerializeOp::makeRenderPaint:
                if (!varUintMatches(opA,
                                    "make_renderpaint_id",
                                    readerA,
                                    readerB))
                {
                    return false;
                }
                break;
            case SerializeOp::decodeImage:
            {
                if (!varUintMatches(opA, "decodeimage_id", readerA, readerB))
                {
                    return false;
                }
                uint64_t size = 0;
                if (!varUintMatches(opA,
                                    "decodeimage_size",
                                    readerA,
                                    readerB,
                                    &size))
                {
                    return false;
                }
                if (!bytesMatches(opA,
                                  "decodeimage_bytes",
                                  size,
                                  readerA,
                                  readerB))
                {
                    return false;
                }
                break;
            }
            case SerializeOp::save:
                break;
            case SerializeOp::restore:
                break;
            case SerializeOp::transform:
                for (int i = 0; i < 6; i++)
                {
                    if (!floatMatches(opA,
                                      std::string("transform[") +
                                          std::to_string(i) + std::string("]"),
                                      readerA,
                                      readerB))
                    {
                        return false;
                    }
                }
                break;
            case SerializeOp::drawPath:
                if (!varUintMatches(opA, "drawpath_id", readerA, readerB))
                {
                    return false;
                }
                if (!varUintMatches(opA, "drawpath_paint_id", readerA, readerB))
                {
                    return false;
                }
                break;
            case SerializeOp::clipPath:
                if (!varUintMatches(opA, "clippath_id", readerA, readerB))
                {
                    return false;
                }
                break;
            case SerializeOp::drawImage:
                if (!varUintMatches(opA, "drawimage_id", readerA, readerB))
                {
                    return false;
                }
                if (!varUintMatches(opA,
                                    "drawimage_blendmode",
                                    readerA,
                                    readerB))
                {
                    return false;
                }
                if (!floatMatches(opA, "drawimage_opacity", readerA, readerB))
                {
                    return false;
                }
                break;
            case SerializeOp::drawImageMesh:
                if (!varUintMatches(opA, "drawimagemesh_id", readerA, readerB))
                {
                    return false;
                }
                if (!varUintMatches(opA,
                                    "drawimagemesh_blendmode",
                                    readerA,
                                    readerB))
                {
                    return false;
                }
                if (!floatMatches(opA,
                                  "drawimagemesh_opacity",
                                  readerA,
                                  readerB))
                {
                    return false;
                }
                if (!varUintMatches(opA,
                                    "drawimagemesh_vertex_id",
                                    readerA,
                                    readerB))
                {
                    return false;
                }
                if (!varUintMatches(opA,
                                    "drawimagemesh_uv_id",
                                    readerA,
                                    readerB))
                {
                    return false;
                }
                if (!varUintMatches(opA,
                                    "drawimagemesh_index_id",
                                    readerA,
                                    readerB))
                {
                    return false;
                }
                break;

            case SerializeOp::setVertexBufferData:
            {
                uint64_t id = 0;
                if (!varUintMatches(opA,
                                    "setvertexbufferdata_id",
                                    readerA,
                                    readerB,
                                    &id))
                {
                    return false;
                }
                if (renderBufferSizes.find(id) == renderBufferSizes.end())
                {
                    fprintf(stderr,
                            "could not find render buffer with id: %" PRIu64
                            "\n",
                            id);
                    return false;
                }
                uint64_t size = renderBufferSizes[id];
                uint64_t floatCount = size / sizeof(float);
                for (int i = 0; i < floatCount; i++)
                {
                    if (!floatMatches(opA,
                                      std::string("setvertexbufferdata_[") +
                                          std::to_string(i) + std::string("]"),
                                      readerA,
                                      readerB))
                    {
                        return false;
                    }
                }
                break;
            }
            case SerializeOp::setIndexBufferData:
            {
                uint64_t id = 0;
                if (!varUintMatches(opA,
                                    "setindexbufferdata_id",
                                    readerA,
                                    readerB,
                                    &id))
                {
                    return false;
                }
                if (renderBufferSizes.find(id) == renderBufferSizes.end())
                {
                    fprintf(stderr,
                            "could not find render buffer with id: %" PRIu64
                            "\n",
                            id);
                    return false;
                }
                uint64_t size = renderBufferSizes[id];
                uint64_t shortCount = size / sizeof(uint16_t);
                for (int i = 0; i < shortCount; i++)
                {
                    if (!shortMatches(opA,
                                      std::string("setindexbufferdata_[") +
                                          std::to_string(i) + std::string("]"),
                                      readerA,
                                      readerB))
                    {
                        return false;
                    }
                }
                break;
            }

            // RenderPath
            case SerializeOp::addRawPath:
            {
                uint64_t verbCount = 0;
                uint64_t pointCount = 0;
                if (!varUintMatches(opA, "addrawpath_id", readerA, readerB))
                {
                    return false;
                }
                if (!varUintMatches(opA,
                                    "addrawpath_verb_count",
                                    readerA,
                                    readerB,
                                    &verbCount))
                {
                    return false;
                }
                for (int i = 0; i < verbCount; i++)
                {
                    if (!varUintMatches(opA,
                                        std::string("addrawpath_verb[") +
                                            std::to_string(i) +
                                            std::string("]"),
                                        readerA,
                                        readerB))
                    {
                        return false;
                    }
                }
                if (!varUintMatches(opA,
                                    "addrawpath_point_count",
                                    readerA,
                                    readerB,
                                    &pointCount))
                {
                    return false;
                }
                for (int i = 0; i < pointCount; i++)
                {
                    if (!vec2DMatches(opA,
                                      std::string("addrawpath_point[") +
                                          std::to_string(i) + std::string("]"),
                                      readerA,
                                      readerB))
                    {
                        return false;
                    }
                }
            }
            break;
            case SerializeOp::rewind:
                if (!varUintMatches(opA, "rewind_path_id", readerA, readerB))
                {
                    return false;
                }
                break;
            case SerializeOp::fillRule:
                if (!varUintMatches(opA, "fillrule_path_id", readerA, readerB))
                {
                    return false;
                }
                if (!varUintMatches(opA, "fillrule_value", readerA, readerB))
                {
                    return false;
                }
                break;

            // RenderPaint
            case SerializeOp::style:
                if (!varUintMatches(opA, "style_paint_id", readerA, readerB))
                {
                    return false;
                }
                if (!varUintMatches(opA, "style_value", readerA, readerB))
                {
                    return false;
                }
                break;
            case SerializeOp::color:
                if (!varUintMatches(opA, "color_paint_id", readerA, readerB))
                {
                    return false;
                }
                if (!varUintMatches(opA, "color_value", readerA, readerB))
                {
                    return false;
                }
                break;
            case SerializeOp::thickness:
                if (!varUintMatches(opA,
                                    "thickness_paint_id",
                                    readerA,
                                    readerB))
                {
                    return false;
                }
                if (!floatMatches(opA, "thickness_value", readerA, readerB))
                {
                    return false;
                }
                break;
            case SerializeOp::join:
                if (!varUintMatches(opA, "join_paint_id", readerA, readerB))
                {
                    return false;
                }
                if (!varUintMatches(opA, "join_value", readerA, readerB))
                {
                    return false;
                }
                break;
            case SerializeOp::cap:
                if (!varUintMatches(opA, "cap_paint_id", readerA, readerB))
                {
                    return false;
                }
                if (!varUintMatches(opA, "cap_value", readerA, readerB))
                {
                    return false;
                }
                break;
            case SerializeOp::feather:
                if (!varUintMatches(opA, "feather_paint_id", readerA, readerB))
                {
                    return false;
                }
                if (!floatMatches(opA, "feather_value", readerA, readerB))
                {
                    return false;
                }
                break;
            case SerializeOp::blendMode:
                if (!varUintMatches(opA,
                                    "blendmode_paint_id",
                                    readerA,
                                    readerB))
                {
                    return false;
                }
                if (!varUintMatches(opA, "blendmode_value", readerA, readerB))
                {
                    return false;
                }
                break;
            case SerializeOp::shader:
                if (!varUintMatches(opA,
                                    "setgradient_paint_id",
                                    readerA,
                                    readerB))
                {
                    return false;
                }
                if (!varUintMatches(opA, "setgradient_value", readerA, readerB))
                {
                    return false;
                }
                break;

            case SerializeOp::frame:
                break;
            case SerializeOp::frameSize:
                if (!varUintMatches(opA, "framesize_width", readerA, readerB))
                {
                    return false;
                }
                if (!varUintMatches(opA, "framesize_height", readerA, readerB))
                {
                    return false;
                }
        }
    }
    if (!readerB.reachedEnd())
    {
        fprintf(stderr, "generated file is longer, otherwise matches.\n");
        return false;
    }
    // for (size_t i = 0; i < length; i++)
    // {
    //     if (fileA[i] != fileB[i])
    //     {
    //         fprintf(stderr,
    //                 "SerializingFactory::matches - %s buffer did not match "
    //                 "generated one.\n",
    //                 filename);

    //         return false;
    //     }
    // }
    return true;
}

bool SerializingFactory::matches(const char* filename)
{
    auto fullFileName =
        std::string("silvers/") + std::string(filename) + std::string(".sriv");
    const char* rebaseline = getenv("REBASELINE_SILVERS");
    if (rebaseline != nullptr)
    {
        save(fullFileName.c_str());
        return true;
    }

    FILE* fp = fopen(fullFileName.c_str(), "rb");
    if (fp == nullptr)
    {
        fprintf(stderr,
                "SerializingFactory::matches - %s is missing\n",
                fullFileName.c_str());
        return false;
    }
    fseek(fp, 0, SEEK_END);
    const size_t length = ftell(fp);
    if (m_buffer.size() != length)
    {
        fprintf(stderr,
                "SerializingFactory::matches - %s size differs from generated "
                "one %zu != %zu.\n",
                filename,
                m_buffer.size(),
                length);
        fclose(fp);
        saveTarnished(filename);
        return false;
    }

    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> existing(length);
    if (fread(existing.data(), 1, length, fp) != length)
    {
        fprintf(stderr,
                "SerializingFactory::matches - %s could not be read.\n",
                filename);
        return false;
    }
    fclose(fp);

    if (!advancedMatch(existing, m_buffer))
    {
        saveTarnished(filename);
        return false;
    }
    return true;
}
