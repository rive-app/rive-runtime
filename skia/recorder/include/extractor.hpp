#ifndef EXTRACTOR_HPP
#define EXTRACTOR_HPP

#include "animation/animation.hpp"
#include "animation/linear_animation_instance.hpp"
#include "animation/linear_animation.hpp"
#include "artboard.hpp"
#include "file.hpp"
#include "SkData.h"
#include "skia_renderer.hpp"
#include "SkImage.h"
#include "SkPixmap.h"
#include "SkStream.h"
#include "SkSurface.h"
#include "util.hpp"
#include "util.hxx"

class RiveFrameExtractor
{
public:
	virtual void extractFrames(int numLoops) const;

	float fps() const;
	int height() const;
	int width() const;
	void takeSnapshot(const std::string& snapshotPath) const;

protected:
	float m_IFps;
	float m_Fps;
	int m_Height;
	int m_MaxDuration;
	int m_MinDuration;
	int m_Width;
	rive::Artboard* m_Artboard;
	rive::File* m_RiveFile;
	rive::LinearAnimation* m_Animation;
	rive::LinearAnimationInstance* m_Animation_instance;
	sk_sp<SkImage> m_WatermarkImage;
	sk_sp<SkSurface> m_RasterSurface;
	SkCanvas* m_RasterCanvas;

	virtual void onNextFrame(int frameNumber) const = 0;

	const void* getPixelAddresses() const;
	int totalFrames() const;
	rive::Artboard* getArtboard(const char* artboard_name) const;
	rive::File* getRiveFile(const char* path) const;
	rive::LinearAnimation* getAnimation(const char* artboard_name) const;
	sk_sp<SkData> getSkData() const;
	sk_sp<SkImage> getSnapshot() const;
	sk_sp<SkImage> getWatermark(const char* watermark_name) const;
	void advanceFrame() const;
	void restart() const;
	void initializeDimensions(int width,
	                          int height,
	                          int small_extent_target,
	                          int max_width,
	                          int max_height);
};

#endif