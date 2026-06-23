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
enum class UseFeather : bool
{
    No,
    Yes,
};

enum class ShapeSize
{
    Small,
    Large,
    Stretch,
};

template <UseFeather Feather,
          ShapeSize InnerSize,
          BlendMode Blend = BlendMode::srcOver>
class ClipShapesGM : public GM
{
public:
    static constexpr int32_t SectionSize = 400;
    static constexpr int32_t Spacing = 50;
    static constexpr int32_t RoundRadius = 50;
    static constexpr int32_t SectionCount = 3;
    static constexpr int32_t ImageDim = SectionCount * SectionSize;

    ClipShapesGM() : GM(ImageDim, ImageDim) {}

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
                    renderer->clipPath(clipper);

                    Paint p;
                    p->color(Colors[xIndex][yIndex]);
                    p->blendMode(Blend);
                    if constexpr (Feather == UseFeather::Yes)
                    {
                        p->feather(float(Spacing));
                    }

                    if constexpr (InnerSize == ShapeSize::Large)
                    {
                        const auto dim =
                            float(SectionCount * SectionSize) * 0.5f;
                        auto path = PathBuilder::Circle(dim, dim, dim * 0.8f);
                        path->fillRule(FillRule::clockwise);
                        renderer->drawPath(path, p);
                    }
                    else
                    {
                        auto x = lerp(box.right(),
                                      box.left(),
                                      float(xIndex) / float(SectionCount - 1));
                        auto y = lerp(box.bottom(),
                                      box.top(),
                                      float(yIndex) / float(SectionCount - 1));
                        auto path =
                            PathBuilder::Circle(x, y, SectionSize * 0.24f);
                        path->fillRule(FillRule::clockwise);
                        renderer->drawPath(path, p);
                    }
                }
                renderer->restore();
            }
        }
    }
};
} // namespace

GMREGISTER(clip_shapes_large_corners,
           return (new ClipShapesGM<UseFeather::No, ShapeSize::Large>()))
GMREGISTER(clip_shapes_large_corners_feathered,
           return (new ClipShapesGM<UseFeather::Yes, ShapeSize::Large>()))
GMREGISTER(clip_shapes_small_corners,
           return (new ClipShapesGM<UseFeather::No, ShapeSize::Small>()))
GMREGISTER(clip_shapes_small_corners_feathered,
           return (new ClipShapesGM<UseFeather::Yes, ShapeSize::Small>()))
GMREGISTER(clip_shapes_large_corners_feathered_blend,
           return (new ClipShapesGM<UseFeather::Yes,
                                    ShapeSize::Large,
                                    BlendMode::screen>()))
GMREGISTER(clip_shapes_small_corners_feathered_blend,
           return (new ClipShapesGM<UseFeather::Yes,
                                    ShapeSize::Small,
                                    BlendMode::screen>()))
