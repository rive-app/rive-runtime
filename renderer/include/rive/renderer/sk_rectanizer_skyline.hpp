/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Initial import from
 * skia:b4171f5ba83048039097bbc664eaa076190f6239@src/gpu/RectanizerSkyline.h
 *
 * Copyright 2025 Rive
 */

#pragma once

#include <cstdint>
#include <vector>

namespace skgpu
{
// Pack rectangles and track the current silhouette
// Based, in part, on Jukka Jylanki's work at http://clb.demon.fi
class RectanizerSkyline
{
public:
    RectanizerSkyline(int w, int h) : fWidth(w), fHeight(h) { this->reset(); }

    int width() const { return fWidth; }
    int height() const { return fHeight; }

    void reset()
    {
        fAreaSoFar = 0;
        fSkyline.clear();
        fSkyline.push_back(SkylineSegment{0, 0, this->width()});
    }

    bool addRect(int w, int h, int16_t* x, int16_t* y);

    bool addPaddedRect(int width,
                       int height,
                       int16_t padding,
                       int16_t* x,
                       int16_t* y)
    {
        if (this->addRect(width + 2 * padding, height + 2 * padding, x, y))
        {
            *x += padding;
            *y += padding;
            return true;
        }
        return false;
    }

    bool empty() const { return fAreaSoFar == 0; }

    float percentFull() const
    {
        return fAreaSoFar / ((float)this->width() * this->height());
    }

private:
    const int fWidth;
    const int fHeight;

    struct SkylineSegment
    {
        int fX;
        int fY;
        int fWidth;
    };

    std::vector<SkylineSegment> fSkyline;

    int32_t fAreaSoFar;

    // Can a width x height rectangle fit in the free space represented by
    // the skyline segments >= 'skylineIndex'? If so, return true and fill in
    // 'y' with the y-location at which it fits (the x location is pulled from
    // 'skylineIndex's segment.
    bool rectangleFits(int skylineIndex, int width, int height, int* y) const;
    // Update the skyline structure to include a width x height rect located
    // at x,y.
    void addSkylineLevel(int skylineIndex, int x, int y, int width, int height);
};
} // End of namespace skgpu
