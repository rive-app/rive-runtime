/*
 * Copyright 2011 Google Inc.
 * Copyright 2022 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"
#include "rive/math/math_types.hpp"

using namespace rivegm;
using namespace rive;

static const float kStdFakeBoldInterpKeys[] = {
    1.0f * 9,
    1.0f * 36,
};
static const float kStdFakeBoldInterpValues[] = {
    1.0f / 24,
    1.0f / 32,
};
static_assert(std::size(kStdFakeBoldInterpKeys) == std::size(kStdFakeBoldInterpValues),
              "mismatched_array_size");
static const int kStdFakeBoldInterpLength = std::size(kStdFakeBoldInterpKeys);

/* Generated on a Mac with:
 * paint.setTypeface(SkTypeface::CreateByName("Papyrus"));
 * paint.getTextPath("H", 1, 100, 80, &textPath);
 */
static Path papyrus_hello()
{
    PathBuilder b;
    b.moveTo(169.824f, 83.4102f);
    b.lineTo(167.285f, 85.6074f);
    b.lineTo(166.504f, 87.2188f);
    b.lineTo(165.82f, 86.7793f);
    b.lineTo(165.186f, 87.1211f);
    b.lineTo(164.6f, 88.1953f);
    b.lineTo(161.914f, 89.416f);
    b.lineTo(161.719f, 89.2207f);
    b.lineTo(160.596f, 88.8789f);
    b.lineTo(160.498f, 87.6094f);
    b.lineTo(160.693f, 86.3887f);
    b.lineTo(161.621f, 84.6797f);
    b.lineTo(161.279f, 83.2148f);
    b.lineTo(161.523f, 81.9941f);
    b.lineTo(162.012f, 79.1133f);
    b.lineTo(162.695f, 76.623f);
    b.lineTo(162.305f, 73.4004f);
    b.lineTo(162.207f, 72.4238f);
    b.lineTo(163.477f, 71.4961f);
    b.quadTo(163.525f, 71.0078f, 163.525f, 70.2754f);
    b.quadTo(163.525f, 68.8594f, 162.793f, 67.1992f);
    b.lineTo(163.623f, 64.6113f);
    b.lineTo(162.598f, 63.6836f);
    b.lineTo(163.184f, 61.0957f);
    b.lineTo(162.695f, 60.8027f);
    b.lineTo(162.988f, 52.5996f);
    b.lineTo(162.402f, 52.1113f);
    b.lineTo(161.914f, 50.1094f);
    b.lineTo(161.523f, 50.5f);
    b.quadTo(160.645f, 50.5f, 160.327f, 50.6465f);
    b.quadTo(160.01f, 50.793f, 159.424f, 51.4766f);
    b.lineTo(158.203f, 52.7949f);
    b.lineTo(156.299f, 52.0137f);
    b.quadTo(154.346f, 52.5996f, 154.004f, 52.5996f);
    b.quadTo(154.053f, 52.5996f, 152.783f, 52.4043f);
    b.quadTo(151.465f, 51.3789f, 150.488f, 51.3789f);
    b.quadTo(150.342f, 51.3789f, 150.269f, 51.4033f);
    b.quadTo(150.195f, 51.4277f, 150.098f, 51.4766f);
    b.lineTo(145.02f, 51.916f);
    b.lineTo(139.893f, 51.2812f);
    b.lineTo(135.693f, 51.623f);
    b.lineTo(133.594f, 51.0859f);
    b.lineTo(130.42f, 52.0137f);
    b.lineTo(125.488f, 51.4766f);
    b.lineTo(124.219f, 51.623f);
    b.lineTo(122.705f, 50.5f);
    b.lineTo(122.217f, 51.7207f);
    b.lineTo(116.211f, 51.623f);
    b.quadTo(114.99f, 52.7949f, 114.99f, 53.8203f);
    b.lineTo(115.576f, 57.6777f);
    b.quadTo(115.723f, 58.2148f, 115.723f, 59.2891f);
    b.quadTo(115.723f, 60.2656f, 115.186f, 61.1934f);
    b.lineTo(115.479f, 64.2207f);
    b.quadTo(114.795f, 64.6602f, 114.648f, 65.0752f);
    b.quadTo(114.502f, 65.4902f, 114.502f, 66.3203f);
    b.quadTo(114.893f, 66.9551f, 115.918f, 67.1992f);
    b.lineTo(116.016f, 69.2988f);
    b.lineTo(116.016f, 75.6953f);
    b.lineTo(116.016f, 75.9883f);
    b.lineTo(116.113f, 77.209f);
    b.quadTo(116.113f, 77.6484f, 115.479f, 78.0879f);
    b.lineTo(115.576f, 79.1133f);
    b.lineTo(116.309f, 82.0918f);
    b.lineTo(116.406f, 83.1172f);
    b.quadTo(116.406f, 85.2656f, 114.404f, 86.291f);
    b.lineTo(111.914f, 87.5117f);
    b.lineTo(109.717f, 88.7812f);
    b.lineTo(108.398f, 89.416f);
    b.lineTo(108.105f, 88.9766f);
    b.lineTo(107.617f, 88.9766f);
    b.quadTo(107.08f, 87.9023f, 107.08f, 87.0234f);
    b.quadTo(107.08f, 85.998f, 107.324f, 85.6074f);
    b.lineTo(108.398f, 83.1172f);
    b.lineTo(108.887f, 81.4082f);
    b.lineTo(109.619f, 81.1152f);
    b.lineTo(109.717f, 79.5039f);
    b.lineTo(108.887f, 78.7227f);
    b.quadTo(109.619f, 77.3555f, 109.619f, 77.1113f);
    b.quadTo(109.619f, 76.8672f, 108.887f, 75.5977f);
    b.lineTo(108.789f, 74.7188f);
    b.lineTo(109.18f, 66.6133f);
    b.lineTo(108.691f, 64.416f);
    b.quadTo(108.691f, 63.293f, 109.521f, 62.707f);
    b.lineTo(110.205f, 62.1211f);
    b.quadTo(110.449f, 61.877f, 110.596f, 60.998f);
    b.quadTo(109.082f, 60.4609f, 109.082f, 59.1914f);
    b.lineTo(109.082f, 58.9961f);
    b.lineTo(109.424f, 55.2852f);
    b.lineTo(108.789f, 53.7227f);
    b.lineTo(108.789f, 48.1074f);
    b.lineTo(108.594f, 45.3242f);
    b.lineTo(108.691f, 43.8105f);
    b.lineTo(107.91f, 43.9082f);
    b.lineTo(107.715f, 44.1035f);
    b.lineTo(107.324f, 42.7852f);
    b.lineTo(107.715f, 43.1758f);
    b.lineTo(107.91f, 41.3203f);
    b.quadTo(109.717f, 40.2949f, 109.717f, 39.2207f);
    b.quadTo(109.717f, 37.707f, 107.715f, 37.4141f);
    b.lineTo(106.982f, 33.9961f);
    b.lineTo(106.982f, 33.2148f);
    b.lineTo(106.689f, 29.3086f);
    b.lineTo(106.689f, 26.1836f);
    b.lineTo(107.91f, 25.6953f);
    b.lineTo(109.521f, 24.5234f);
    b.lineTo(112.109f, 23.3027f);
    b.lineTo(114.014f, 22.5215f);
    b.lineTo(115.479f, 23.1074f);
    b.quadTo(115.479f, 24.0352f, 116.113f, 25.1094f);
    b.quadTo(115.381f, 25.8906f, 115.259f, 26.3789f);
    b.quadTo(115.137f, 26.8672f, 114.99f, 28.8203f);
    b.lineTo(114.893f, 29.9922f);
    b.lineTo(115.82f, 33.4102f);
    b.lineTo(116.016f, 35.4121f);
    b.lineTo(115.576f, 35.4121f);
    b.lineTo(114.697f, 37.707f);
    b.lineTo(115.381f, 38.4883f);
    b.lineTo(115.186f, 39.5137f);
    b.lineTo(114.697f, 40.3926f);
    b.lineTo(114.209f, 41.418f);
    b.lineTo(114.404f, 42.4922f);
    b.lineTo(114.795f, 43.3223f);
    b.quadTo(115.186f, 44.1035f, 115.186f, 46.4961f);
    b.lineTo(116.309f, 46.7891f);
    b.lineTo(125, 47.2773f);
    b.lineTo(126.318f, 48.2051f);
    b.lineTo(129.59f, 48.5957f);
    b.lineTo(130.615f, 48.498f);
    b.lineTo(130.908f, 48.0098f);
    b.lineTo(134.277f, 48.2051f);
    b.lineTo(134.717f, 48.3027f);
    b.quadTo(135.4f, 47.7168f, 136.084f, 47.7168f);
    b.lineTo(137.109f, 47.8145f);
    b.lineTo(137.109f, 48.2051f);
    b.lineTo(137.695f, 48.5957f);
    b.lineTo(138.818f, 48.498f);
    b.lineTo(144.189f, 48.4004f);
    b.lineTo(146.191f, 48.2051f);
    b.lineTo(147.998f, 48.791f);
    b.quadTo(148.877f, 47.5215f, 150.098f, 47.5215f);
    b.lineTo(150.293f, 47.5215f);
    b.lineTo(152.783f, 47.7168f);
    b.lineTo(157.52f, 47.2773f);
    b.lineTo(160.303f, 47.4238f);
    b.lineTo(161.279f, 47.1797f);
    b.lineTo(161.621f, 46.1055f);
    b.lineTo(162.5f, 43.9082f);
    b.lineTo(162.305f, 38.293f);
    b.lineTo(162.5f, 37.1211f);
    b.lineTo(161.816f, 35.998f);
    b.lineTo(160.596f, 34.7773f);
    b.lineTo(160.596f, 32.6777f);
    b.lineTo(160.303f, 30.4805f);
    b.lineTo(160.889f, 29.4062f);
    b.quadTo(160.303f, 27.4043f, 160.303f, 26.2812f);
    b.quadTo(160.303f, 25.3535f, 160.889f, 25.207f);
    b.lineTo(162.207f, 25.0117f);
    b.lineTo(163.721f, 23.8887f);
    b.lineTo(164.893f, 23.8887f);
    b.quadTo(165.625f, 23.6934f, 166.797f, 22.5215f);
    b.lineTo(168.213f, 23.2051f);
    b.lineTo(168.701f, 25.5f);
    b.lineTo(169.092f, 26.8184f);
    b.lineTo(167.676f, 31.9941f);
    b.lineTo(168.018f, 34.6797f);
    b.lineTo(167.822f, 35.8027f);
    b.lineTo(167.285f, 35.8027f);
    b.quadTo(166.602f, 35.8027f, 166.602f, 36.7793f);
    b.quadTo(166.846f, 37.3652f, 167.578f, 37.707f);
    b.lineTo(168.506f, 38.1953f);
    b.lineTo(168.799f, 39.5137f);
    b.lineTo(169.092f, 41.5156f);
    b.lineTo(168.994f, 42.1016f);
    b.lineTo(168.213f, 43.6152f);
    b.lineTo(168.408f, 52.502f);
    b.lineTo(168.213f, 60.1191f);
    b.lineTo(168.994f, 61.291f);
    b.quadTo(168.604f, 63.0488f, 168.604f, 63.9766f);
    b.lineTo(168.604f, 64.123f);
    b.lineTo(168.604f, 64.3184f);
    b.lineTo(168.604f, 64.9043f);
    b.lineTo(168.604f, 66.0762f);
    b.quadTo(167.578f, 67.1504f, 167.578f, 67.5898f);
    b.quadTo(167.578f, 67.7852f, 167.676f, 67.7852f);
    b.lineTo(168.994f, 70.0801f);
    b.lineTo(168.701f, 76.7207f);
    b.lineTo(169.824f, 83.4102f);
    b.close();
    return b.detach();
}

/* Generated on a Mac with:
 * paint.setTypeface(SkTypeface::CreateByName("Hiragino Maru Gothic Pro"));
 * const unsigned char hyphen[] = { 0xE3, 0x83, 0xBC };
 * paint.getTextPath(hyphen, SK_ARRAY_COUNT(hyphen), 400, 80, &textPath);
 */
static Path hiragino_maru_gothic_pro_dash()
{
    Path path;
    path->moveTo(488, 55.1f);
    path->cubicTo(490.5f, 55.1f, 491.9f, 53.5f, 491.9f, 50.8f);
    path->cubicTo(491.9f, 48.2f, 490.5f, 46.3f, 487.9f, 46.3f);
    path->lineTo(412, 46.3f);
    path->cubicTo(409.4f, 46.3f, 408, 48.2f, 408, 50.8f);
    path->cubicTo(408, 53.5f, 409.4f, 55.1f, 411.9f, 55.1f);
    path->lineTo(488, 55.1f);
    path->close();
    return path;
}

float lerp(float a, float b, float t) { return a + t * (b - a); }

float scalarInterpFunc(float searchKey, const float keys[], const float values[], int length)
{
    assert(length > 0);
    assert(keys != nullptr);
    assert(values != nullptr);
#ifdef RIVE_DEBUG
    for (int i = 1; i < length; i++)
    {
        assert(keys[i - 1] <= keys[i]);
    }
#endif
    int right = 0;
    while (right < length && keys[right] < searchKey)
    {
        ++right;
    }
    // Could use sentinel values to eliminate conditionals, but since the
    // tables are taken as input, a simpler format is better.
    if (right == length)
    {
        return values[length - 1];
    }
    if (right == 0)
    {
        return values[0];
    }
    // Otherwise, interpolate between right - 1 and right.
    float leftKey = keys[right - 1];
    float rightKey = keys[right];
    float fract = (searchKey - leftKey) / (rightKey - leftKey);
    return lerp(values[right - 1], values[right], fract);
}

static void path_bold(Renderer* canvas, const Path& path, float textSize)
{
    Paint p;
    p->thickness(static_cast<float>(5));
    canvas->drawPath(path, p);
    float fakeBoldScale = scalarInterpFunc(textSize,
                                           kStdFakeBoldInterpKeys,
                                           kStdFakeBoldInterpValues,
                                           kStdFakeBoldInterpLength);
    float extra = textSize * fakeBoldScale;
    p->thickness(extra);
    canvas->save();
    canvas->translate(0, 120);
    canvas->drawPath(path, p);
    p->style(RenderPaintStyle::stroke);
    canvas->drawPath(path, p);
    canvas->restore();
}

static AABB xywh(float x, float y, float w, float h) { return {x, y, x + w, y + h}; }

DEF_SIMPLE_GM(strokefill, 640, 480, canvas)
{
    float x = static_cast<float>(100);
    float y = static_cast<float>(88);

    Paint paint;
    paint->thickness(static_cast<float>(5));

    // use paths instead of text to test the path data on all platforms, since the
    // Mac-specific font may change or is not available everywhere
    path_bold(canvas, papyrus_hello(), 100);
    path_bold(canvas, hiragino_maru_gothic_pro_dash(), 100);

    PathBuilder b;
    b.fillRule(FillRule::nonZero);
    b.addCircle(x, y + static_cast<float>(200), static_cast<float>(50), rivegm::PathDirection::cw);
    b.addCircle(x, y + static_cast<float>(200), static_cast<float>(40), rivegm::PathDirection::ccw);
    canvas->drawPath(b.detach(), paint);

    b.addCircle(x + static_cast<float>(120),
                y + static_cast<float>(200),
                static_cast<float>(50),
                rivegm::PathDirection::ccw);
    b.addCircle(x + static_cast<float>(120),
                y + static_cast<float>(200),
                static_cast<float>(40),
                rivegm::PathDirection::cw);
    canvas->drawPath(b.detach(), paint);

    b.addCircle(x + static_cast<float>(240),
                y + static_cast<float>(200),
                static_cast<float>(50),
                rivegm::PathDirection::ccw);
    canvas->drawPath(b.detach(), paint);

    b.addCircle(x + static_cast<float>(360),
                y + static_cast<float>(200),
                static_cast<float>(50),
                rivegm::PathDirection::cw);
    canvas->drawPath(b.detach(), paint);

    AABB r = xywh(x - static_cast<float>(50),
                  y + static_cast<float>(280),
                  static_cast<float>(100),
                  static_cast<float>(100));
    b.addRect(r, rivegm::PathDirection::cw);
    r = r.inset(static_cast<float>(10), static_cast<float>(10));
    b.addRect(r, rivegm::PathDirection::ccw);
    canvas->drawPath(b.detach(), paint);

    r = xywh(x + static_cast<float>(70),
             y + static_cast<float>(280),
             static_cast<float>(100),
             static_cast<float>(100));
    b.addRect(r, rivegm::PathDirection::ccw);
    r = r.inset(static_cast<float>(10), static_cast<float>(10));
    b.addRect(r, rivegm::PathDirection::cw);
    canvas->drawPath(b.detach(), paint);

    r = xywh(x + static_cast<float>(190),
             y + static_cast<float>(280),
             static_cast<float>(100),
             static_cast<float>(100));
    b.addRect(r, rivegm::PathDirection::ccw);
    b.moveTo(0, 0); // test for crbug.com/247770
    canvas->drawPath(b.detach(), paint);

    r = xywh(x + static_cast<float>(310),
             y + static_cast<float>(280),
             static_cast<float>(100),
             static_cast<float>(100));
    b.addRect(r, rivegm::PathDirection::cw);
    b.moveTo(0, 0); // test for crbug.com/247770
    canvas->drawPath(b.detach(), paint);
}

DEF_SIMPLE_GM(bug339297, 640, 480, canvas)
{
    Path path;
    path->moveTo(-469515, -10354890);
    path->cubicTo(771919.62f, -10411179, 2013360.1f, -10243774, 3195542.8f, -9860664);
    path->lineTo(3195550, -9860655);
    path->lineTo(3195539, -9860652);
    path->lineTo(3195539, -9860652);
    path->lineTo(3195539, -9860652);
    path->cubicTo(2013358.1f, -10243761, 771919.25f, -10411166, -469513.84f, -10354877);
    path->lineTo(-469515, -10354890);
    path->close();

    canvas->translate(258, 10365663);

    Paint paint;
    paint->color(0xff000000);
    paint->style(RenderPaintStyle::fill);
    canvas->drawPath(path, paint);

    paint->color(0xffff0000);
    paint->style(RenderPaintStyle::stroke);
    paint->thickness(1);
    canvas->drawPath(path, paint);
}

DEF_SIMPLE_GM(bug339297_as_clip, 640, 480, canvas)
{
    Path path;
    path->moveTo(-469515, -10354890);
    path->cubicTo(771919.62f, -10411179, 2013360.1f, -10243774, 3195542.8f, -9860664);
    path->lineTo(3195550, -9860655);
    path->lineTo(3195539, -9860652);
    path->lineTo(3195539, -9860652);
    path->lineTo(3195539, -9860652);
    path->cubicTo(2013358.1f, -10243761, 771919.25f, -10411166, -469513.84f, -10354877);
    path->lineTo(-469515, -10354890);
    path->close();

    canvas->translate(258, 10365663);

    canvas->save();
    canvas->clipPath(path);
    Paint clearPaint;
    clearPaint->color(0xff000000);
    const float b = 1e9f;
    draw_rect(canvas, {-b, -b, b, b}, clearPaint);
    canvas->restore();

    Paint paint;
    paint->style(RenderPaintStyle::fill);
    paint->color(0xffff0000);
    paint->style(RenderPaintStyle::stroke);
    paint->thickness(1);
    canvas->drawPath(path, paint);
}

DEF_SIMPLE_GM(bug6987, 75, 75, canvas)
{
    Paint paint;
    paint->style(RenderPaintStyle::stroke);
    paint->thickness(0.0001f);
    Path path;
    canvas->save();
    canvas->scale(50000.0f, 50000.0f);
    path->moveTo(0.0005f, 0.0004f);
    path->lineTo(0.0008f, 0.0010f);
    path->lineTo(0.0002f, 0.0010f);
    path->close();
    canvas->drawPath(path, paint);
    canvas->restore();
}
