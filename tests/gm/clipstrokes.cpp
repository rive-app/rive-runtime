/*
 * Copyright 2026 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/math/math_types.hpp"
#include "rive/renderer.hpp"
#include "common/rand.hpp"

using namespace rivegm;
using namespace rive;

namespace
{
class ClipStrokeGM : public GM
{
public:
    static constexpr int32_t SectionSize = 400;
    static constexpr int32_t Spacing = 50;
    static constexpr int32_t RoundRadius = 50;
    static constexpr int32_t SectionCount = 3;
    static constexpr int32_t ImageDim = SectionCount * SectionSize;

    ClipStrokeGM() : GM(ImageDim, ImageDim) {}

    void onDraw(rive::Renderer* renderer) override
    {
        {
            Paint p;
            p->color(0xff000000);
            renderer->drawPath(
                PathBuilder::Rect(
                    {0.0f, 0.0f, float(ImageDim), float(ImageDim)}),
                p);
        }
        {
            Paint p;
            p->color(0xff2c1642);
            p->feather(10.0f);
            auto yStep = 40.0f;
            for (auto y = 20.0f; y < float(SectionCount * SectionSize); y++)
            {
                auto path = PathBuilder::Rect({
                    -100.0f,
                    y,
                    float(ImageDim) + 100.0f,
                    y + yStep * 0.3f,
                });
                path->fillRule(FillRule::clockwise);
                renderer->drawPath(path, p);
                y += yStep;
                yStep *= 1.12f;
            }
        }
        constexpr uint32_t Colors[SectionCount][SectionCount] = {
            {
                0xff881177,
                0xffeedd00,
                0xff00bbcc,
            },
            {
                0xffaa3355,
                0xff99dd55,
                0xffee9944,
            },
            {
                0xffcc6666,
                0xff3366bb,
                0xff22ccbb,
            },
        };
        for (auto yIndex = 0; yIndex < 3; yIndex++)
        {
            for (auto xIndex = 0; xIndex < 3; xIndex++)
            {
                renderer->save();
                {
                    const auto box = AABB{
                        float(SectionSize * xIndex + Spacing / 2),
                        float(SectionSize * yIndex + Spacing / 2),
                        float(SectionSize * (xIndex + 1) - Spacing / 2),
                        float(SectionSize * (yIndex + 1) - Spacing / 2),
                    };
                    auto clipper = PathBuilder::RRect(box,
                                                      float(RoundRadius),
                                                      float(RoundRadius));
                    renderer->clipStroke(
                        clipper,
                        {.thickness = float(yIndex * 3 + xIndex + 1) * 3.0f});

                    Paint p;
                    p->color(Colors[xIndex][yIndex]);

                    const auto dim = float(SectionCount * SectionSize) * 0.5f;
                    auto path = PathBuilder::Circle(dim, dim, dim * 0.8f);
                    path->fillRule(FillRule::clockwise);
                    renderer->drawPath(path, p);
                }
                renderer->restore();
            }
        }
    }
};

enum class NestType
{
    pathInStroke,
    strokeInPath,
    strokeInStroke,
};

template <NestType N> class ClipStrokeNestedGM : public GM
{
public:
    static constexpr int32_t SectionSize = 400;
    static constexpr int32_t Spacing = 50;
    static constexpr int32_t RoundRadius = 50;
    static constexpr int32_t SectionCount = 3;
    static constexpr int32_t ImageDim = SectionCount * SectionSize;

    ClipStrokeNestedGM() : GM(ImageDim, ImageDim) {}

    void onDraw(rive::Renderer* renderer) override
    {
        {
            Paint p;
            p->color(0xff000000);
            renderer->drawPath(
                PathBuilder::Rect(
                    {0.0f, 0.0f, float(ImageDim), float(ImageDim)}),
                p);
        }
        {
            Paint p;
            p->color(0xff2c1642);
            p->feather(10.0f);
            auto yStep = 40.0f;
            for (auto y = 20.0f; y < float(SectionCount * SectionSize); y++)
            {
                auto path = PathBuilder::Rect({
                    -100.0f,
                    y,
                    float(ImageDim) + 100.0f,
                    y + yStep * 0.3f,
                });
                path->fillRule(FillRule::clockwise);
                renderer->drawPath(path, p);
                y += yStep;
                yStep *= 1.12f;
            }
        }
        constexpr uint32_t Colors[] = {
            0xffeedd00,
            0xffee9944,
            0xffcc6666,
            0xffaa3355,
            0xff881177,
            0xff3366bb,
            0xff00bbcc,
            0xff22ccbb,
            0xff99dd55,
        };

        const auto dim = float(SectionCount * SectionSize) * 0.5f;
        auto outerPath = PathBuilder::Circle(dim, dim, dim * 0.8f);

        constexpr auto CircleDiameter = 100;
        constexpr auto CircleSpacing = 20;
        constexpr auto CircleRadius = float(CircleDiameter) * 0.5f;
        auto innerCircle =
            PathBuilder::Circle(CircleRadius, CircleRadius, CircleRadius);

        renderer->save();
        {
            if constexpr (N == NestType::strokeInPath)
            {
                renderer->clipPath(outerPath);
            }
            else
            {
                renderer->clipStroke(outerPath, {.thickness = 200.0f});
            }

            for (auto yIndex = 0; yIndex < 3; yIndex++)
            {
                for (auto xIndex = 0; xIndex < 3; xIndex++)
                {
                    renderer->save();
                    {
                        const auto box = AABB{
                            float(SectionSize * xIndex + Spacing / 2),
                            float(SectionSize * yIndex + Spacing / 2),
                            float(SectionSize * (xIndex + 1) - Spacing / 2),
                            float(SectionSize * (yIndex + 1) - Spacing / 2),
                        };
                        auto clipper = PathBuilder::RRect(box,
                                                          float(RoundRadius),
                                                          float(RoundRadius));

                        if constexpr (N == NestType::pathInStroke)
                        {
                            renderer->clipPath(clipper);
                        }
                        else
                        {
                            renderer->clipStroke(clipper, {.thickness = 20.0f});
                        }

                        auto colorIndex = 0u;
                        for (auto y = 5; y < ImageDim;
                             y += CircleDiameter + CircleSpacing)
                        {
                            for (auto x = 5; x < ImageDim;
                                 x += CircleDiameter + CircleSpacing)
                            {
                                Paint p;
                                p->color(
                                    Colors[colorIndex % std::size(Colors)]);
                                colorIndex++;
                                renderer->save();
                                {
                                    renderer->translate(x, y);
                                    renderer->drawPath(innerCircle, p);
                                }
                                renderer->restore();
                            }
                        }
                    }

                    renderer->restore();
                }
            }
        }
        renderer->restore();
    }
};

class ClipStrokeOpenGM : public GM
{
public:
    static constexpr int32_t SectionSize = 400;
    static constexpr int32_t SectionCount = 3;
    static constexpr int32_t ImageDim = SectionCount * SectionSize;
    static constexpr int32_t StrokeWidth = 50.0f;

    ClipStrokeOpenGM() : GM(ImageDim, ImageDim) {}

    void onDraw(rive::Renderer* renderer) override
    {
        Path clipper =
            PathBuilder()
                .addPolygon(
                    std::vector{Vec2D{StrokeWidth, StrokeWidth},
                                Vec2D{SectionSize - StrokeWidth, StrokeWidth},
                                Vec2D{SectionSize - StrokeWidth,
                                      SectionSize - StrokeWidth},
                                Vec2D{StrokeWidth, SectionSize - StrokeWidth}},
                    false)
                .detach();

        {
            Paint p;
            p->color(0xff000000);
            renderer->drawPath(
                PathBuilder::Rect(
                    {0.0f, 0.0f, float(ImageDim), float(ImageDim)}),
                p);
        }

        {
            Paint p;
            p->color(0xff2c1642);
            p->feather(10.0f);
            auto yStep = 40.0f;
            for (auto y = 20.0f; y < float(SectionCount * SectionSize); y++)
            {
                auto path = PathBuilder::Rect({
                    -100.0f,
                    y,
                    float(ImageDim) + 100.0f,
                    y + yStep * 0.3f,
                });
                path->fillRule(FillRule::clockwise);
                renderer->drawPath(path, p);
                y += yStep;
                yStep *= 1.12f;
            }
        }
        constexpr uint32_t Colors[SectionCount][SectionCount] = {
            {
                0xff881177,
                0xffeedd00,
                0xff00bbcc,
            },
            {
                0xffaa3355,
                0xff99dd55,
                0xffee9944,
            },
            {
                0xffcc6666,
                0xff3366bb,
                0xff22ccbb,
            },
        };
        for (auto yIndex = 0; yIndex < SectionCount; yIndex++)
        {
            for (auto xIndex = 0; xIndex < SectionCount; xIndex++)
            {
                renderer->save();
                {
                    const auto tX = float(SectionSize * xIndex);
                    const auto tY = float(SectionSize * yIndex);
                    renderer->translate(tX, tY);
                    renderer->clipStroke(clipper,
                                         {
                                             .thickness = StrokeWidth,
                                             .join = StrokeJoin(xIndex),
                                             .cap = StrokeCap(yIndex),
                                         });

                    Paint p;
                    p->color(Colors[xIndex][yIndex]);

                    renderer->translate(-tX, -tY);

                    const auto dim = float(SectionCount * SectionSize) * 0.5f;
                    auto path = PathBuilder::Circle(dim, dim, dim * 5.0f);
                    path->fillRule(FillRule::clockwise);
                    renderer->drawPath(path, p);
                }
                renderer->restore();
            }
        }
    }
};
} // namespace

GMREGISTER(clip_stroke_basic, return (new ClipStrokeGM()))
GMREGISTER(clip_stroke_nested_a,
           return (new ClipStrokeNestedGM<NestType::pathInStroke>()))
GMREGISTER(clip_stroke_nested_b,
           return (new ClipStrokeNestedGM<NestType::strokeInPath>()))
GMREGISTER(clip_stroke_nested_c,
           return (new ClipStrokeNestedGM<NestType::strokeInStroke>()))
GMREGISTER(clip_stroke_open, return (new ClipStrokeOpenGM()))
