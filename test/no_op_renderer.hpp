#ifndef _RIVE_NOOP_RENDERER_HPP_
#define _RIVE_NOOP_RENDERER_HPP_
#include <rive/renderer.hpp>
#include <vector>

namespace rive
{
    class NoOpRenderImage : public RenderImage
    {
    public:
        bool decode(const uint8_t* bytes, std::size_t size) override
        {
            return true;
        }
    };

    class NoOpRenderPaint : public RenderPaint
    {
    public:
        void color(unsigned int value) override {}
        void style(RenderPaintStyle value) override {}
        void thickness(float value) override {}
        void join(StrokeJoin value) override {}
        void cap(StrokeCap value) override {}
        void blendMode(BlendMode value) override {}

        void linearGradient(float sx, float sy, float ex, float ey) override {}
        void radialGradient(float sx, float sy, float ex, float ey) override {}
        void addStop(unsigned int color, float stop) override {}
        void completeGradient() override {}
    };

    enum class NoOpPathCommandType
    {
        MoveTo,
        LineTo,
        CubicTo,
        Reset,
        Close
    };

    struct NoOpPathCommand
    {
        NoOpPathCommandType command;
        float x;
        float y;
        float inX;
        float inY;
        float outX;
        float outY;
    };

    class NoOpRenderPath : public RenderPath
    {
    public:
        std::vector<NoOpPathCommand> commands;
        void reset() override
        {
            commands.emplace_back((NoOpPathCommand){NoOpPathCommandType::Reset,
                                                    0.0f,
                                                    0.0f,
                                                    0.0f,
                                                    0.0f,
                                                    0.0f,
                                                    0.0f});
        }

        void fillRule(FillRule value) override {}
        void addPath(CommandPath* path, const Mat2D& transform) override {}
        void addRenderPath(RenderPath* path, const Mat2D& transform) override {}

        void moveTo(float x, float y) override
        {
            commands.emplace_back((NoOpPathCommand){
                NoOpPathCommandType::MoveTo, x, y, 0.0f, 0.0f, 0.0f, 0.0f});
        }
        void lineTo(float x, float y) override
        {
            commands.emplace_back((NoOpPathCommand){
                NoOpPathCommandType::LineTo, x, y, 0.0f, 0.0f, 0.0f, 0.0f});
        }
        void cubicTo(
            float ox, float oy, float ix, float iy, float x, float y) override
        {
            commands.emplace_back((NoOpPathCommand){
                NoOpPathCommandType::CubicTo, x, y, ix, iy, ox, oy});
        }
        void close() override
        {
            commands.emplace_back((NoOpPathCommand){NoOpPathCommandType::Close,
                                                    0.0f,
                                                    0.0f,
                                                    0.0f,
                                                    0.0f,
                                                    0.0f,
                                                    0.0f});
        }
    };

    class NoOpRenderer : public Renderer
    {
        void save() override {}
        void restore() override {}
        void transform(const Mat2D& transform) override {}
        void drawPath(RenderPath* path, RenderPaint* paint) override {}
        void clipPath(RenderPath* path) override {}
        void
        drawImage(RenderImage* image, BlendMode value, float opacity) override
        {
        }
    };

} // namespace rive
#endif