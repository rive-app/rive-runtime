#ifndef EXTRACTOR_HPP
#define EXTRACTOR_HPP

#include "SkImage.h"
#include "animation/linear_animation.hpp"
#include "artboard.hpp"
#include "file.hpp"
#include "util.hxx"

class RiveFrameExtractor
{

public:
	rive::Artboard* artboard;
	rive::LinearAnimation* animation;
	sk_sp<SkImage> watermarkImage;
	RiveFrameExtractor(const char* path,
	                   const char* artboard_name,
	                   const char* animation_name,
	                   const char* watermark_name);

private:
	rive::File* riveFile;
	sk_sp<SkImage> getWaterMark(const char* watermark_name);
	rive::File* getRiveFile(const char* path);
	rive::Artboard* getArtboard(const char* artboard_name);
	rive::LinearAnimation* getAnimation(const char* animation_name);
};

#endif