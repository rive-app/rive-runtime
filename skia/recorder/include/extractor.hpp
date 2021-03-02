#ifndef EXTRACTOR_HPP
#define EXTRACTOR_HPP

#include "SkData.h"
#include "SkImage.h"
#include "SkPixmap.h"
#include "SkStream.h"
#include "SkSurface.h"
#include "animation/animation.hpp"
#include "animation/linear_animation.hpp"
#include "animation/linear_animation_instance.hpp"
#include "artboard.hpp"
#include "core/binary_reader.hpp"
#include "file.hpp"
#include "math/aabb.hpp"
#include "skia_renderer.hpp"
#include "util.hpp"
#include "util.hxx"

class RiveFrameExtractor
{

public:
	RiveFrameExtractor(const char* path,
	                   const char* artboard_name,
	                   const char* animation_name,
	                   const char* watermark_name,
	                   int width = 0,
	                   int height = 0,
	                   int small_extent_target = 0,
	                   int max_width = 0,
	                   int max_height = 0,
	                   int min_duration = 0,
	                   int max_duration = 0);
	int width();
	int height();
	int fps();
	int totalFrames();
	const void* getFrame(int i);

private:
	int _width, _height, _min_duration, _max_duration;
	rive::File* riveFile;
	float ifps;
	sk_sp<SkImage> getWaterMark(const char* watermark_name);
	rive::File* getRiveFile(const char* path);
	rive::Artboard* getArtboard(const char* artboard_name);
	rive::LinearAnimation* getAnimation(const char* artboard_name);
	rive::Artboard* artboard;
	rive::LinearAnimation* animation;
	rive::LinearAnimationInstance* animation_instance;
	sk_sp<SkImage> watermarkImage;
	SkCanvas* rasterCanvas;
	sk_sp<SkSurface> rasterSurface;
	void initializeDimensions(int width,
	                          int height,
	                          int small_extent_target,
	                          int max_width,
	                          int max_height);
};

#endif