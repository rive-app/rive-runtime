/*
 * Copyright 2022 Rive
 */

#include "rive/factory.hpp"
#include "rive/math/aabb.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/text/raw_text.hpp"
#ifdef WITH_RIVE_TEXT
#include "rive/text/font_hb.hpp"
#endif

using namespace rive;

rcp<RenderPath> Factory::makeRenderPath(const AABB& r)
{
    RawPath rawPath;
    rawPath.addRect(r);
    return makeRenderPath(rawPath, FillRule::nonZero);
}

rcp<Font> Factory::decodeFont(Span<const uint8_t> span)
{
#ifdef WITH_RIVE_TEXT
    return HBFont::Decode(span);
#else
    return nullptr;
#endif
}

rcp<AudioSource> Factory::decodeAudio(Span<const uint8_t> span)
{
#ifdef WITH_RIVE_AUDIO
    return rcp<AudioSource>(new AudioSource(SimpleArray<uint8_t>(span.data(), span.size())));
#else
    return nullptr;
#endif
}
