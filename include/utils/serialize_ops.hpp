/*
 * Copyright 2026 Rive
 */

#ifndef _RIVE_SERIALIZE_OPS_HPP_
#define _RIVE_SERIALIZE_OPS_HPP_

#include "rive/core/binary_reader.hpp"
#include "rive/core/binary_writer.hpp"
#include "rive/math/raw_path.hpp"
#include <vector>

namespace rive
{
// Wire opcodes shared by SerializingFactory and replaySerializedCommands so
// the two ends of the .sriv format cannot drift apart.
enum class SerializeOp : uint32_t
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
    modulateOpacity = 30,
};

inline void serializeRawPath(BinaryWriter* writer, const RawPath& path)
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

inline RawPath deserializeRawPath(BinaryReader& reader)
{
    RawPath path;
    size_t verbCount = static_cast<size_t>(reader.readVarUint64());
    std::vector<PathVerb> verbs(verbCount);
    for (size_t i = 0; i < verbCount; ++i)
        verbs[i] = static_cast<PathVerb>(reader.readVarUint64());
    size_t pointCount = static_cast<size_t>(reader.readVarUint64());
    std::vector<Vec2D> pts(pointCount);
    for (size_t i = 0; i < pointCount; ++i)
    {
        pts[i].x = reader.readFloat32();
        pts[i].y = reader.readFloat32();
    }
    size_t p = 0;
    // A truncated stream can promise more points than it delivers.
    auto have = [&](size_t n) { return p + n <= pts.size(); };
    for (PathVerb v : verbs)
    {
        switch (v)
        {
            case PathVerb::move:
                if (!have(1))
                    return path;
                path.move(pts[p++]);
                break;
            case PathVerb::line:
                if (!have(1))
                    return path;
                path.line(pts[p++]);
                break;
            case PathVerb::quad:
                if (!have(2))
                    return path;
                path.quad(pts[p], pts[p + 1]);
                p += 2;
                break;
            case PathVerb::cubic:
                if (!have(3))
                    return path;
                path.cubic(pts[p], pts[p + 1], pts[p + 2]);
                p += 3;
                break;
            case PathVerb::close:
                path.close();
                break;
        }
    }
    return path;
}
} // namespace rive

#endif
