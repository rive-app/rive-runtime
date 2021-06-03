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
	                   int max_duration = 0,
	                   float fps = 0);
	~RiveFrameExtractor();

	int width() const;
	int height() const;
	int totalFrames() const;
	float fps() const;
	void advanceFrame() const;
	void restart() const;
	const void* getPixelAddresses() const;
	sk_sp<SkData> getSkData() const;

private:
	int m_Width, m_Height, m_MinDuration, m_MaxDuration;
	rive::File* m_RiveFile;
	float ifps, m_Fps;
	rive::Artboard* m_Artboard;
	rive::LinearAnimation* m_Animation;
	rive::LinearAnimationInstance* m_Animation_instance;
	sk_sp<SkImage> m_WatermarkImage;
	SkCanvas* m_RasterCanvas;
	sk_sp<SkSurface> m_RasterSurface;

	sk_sp<SkImage> getWatermark(const char* watermark_name) const;
	rive::File* getRiveFile(const char* path) const;
	rive::Artboard* getArtboard(const char* artboard_name) const;
	rive::LinearAnimation* getAnimation(const char* artboard_name) const;
	sk_sp<SkImage> getSnapshot() const;
	void initializeDimensions(int width,
	                          int height,
	                          int small_extent_target,
	                          int max_width,
	                          int max_height);
};

#endif