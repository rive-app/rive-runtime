#ifndef EXTRACTOR_HPP
#define EXTRACTOR_HPP

#include "SkData.h"
#include "SkImage.h"
#include "SkPixmap.h"
#include "SkStream.h"
#include "SkSurface.h"
#include "animation/animation.hpp"
#include "animation/linear_animation.hpp"
#include "artboard.hpp"
#include "core/binary_reader.hpp"
#include "file.hpp"
#include "math/aabb.hpp"
#include "skia_renderer.hpp"
#include "util.hxx"

class RiveFrameExtractor
{

public:
	rive::Artboard* artboard;
	rive::LinearAnimation* animation;
	sk_sp<SkImage> watermarkImage;
	SkCanvas* rasterCanvas;
	sk_sp<SkSurface> rasterSurface;
	RiveFrameExtractor(const char* path,
	                   const char* artboard_name,
	                   const char* animation_name,
	                   const char* watermark_name);
	int width() { return artboard->width(); };
	int height() { return artboard->height(); };
	const void* getFrame(int i);

private:
	rive::File* riveFile;
	float ifps;
	sk_sp<SkImage> getWaterMark(const char* watermark_name);
	rive::File* getRiveFile(const char* path);
	rive::Artboard* getArtboard(const char* artboard_name);
	rive::LinearAnimation* getAnimation(const char* animation_name);
};

#endif