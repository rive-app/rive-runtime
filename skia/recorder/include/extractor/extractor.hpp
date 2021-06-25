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
	RiveFrameExtractor(const std::string& path,
	                   const std::string& artboardName,
	                   const std::string& animationName,
	                   const std::string& watermark,
	                   const std::string& destination,
	                   int width = 0,
	                   int height = 0,
	                   int smallExtentTarget = 0,
	                   int maxWidth = 0,
	                   int maxHeight = 0,
	                   int duration = 0,
	                   int minDuration = 0,
	                   int maxDuration = 0,
	                   int fps = 0) :
	    m_MinDuration(minDuration),
	    m_MaxDuration(maxDuration),
	    m_RiveFile(getRiveFile(path.c_str())),
	    m_Artboard(getArtboard(artboardName.c_str())),
	    m_Animation(getAnimation(animationName.c_str())),
	    m_AnimationInstance(new rive::LinearAnimationInstance(m_Animation)),
	    m_WatermarkImage(getWatermark(watermark.c_str()))
	{
		initializeDimensions(
		    width, height, smallExtentTarget, maxWidth, maxHeight);
		m_RasterSurface = SkSurface::MakeRaster(SkImageInfo::Make(
		    m_Width, m_Height, kRGBA_8888_SkColorType, kPremul_SkAlphaType));
		m_RasterCanvas = m_RasterSurface->getCanvas();
		m_Fps = valueOrDefault(fps, m_Animation->fps());
		m_IFps = 1.0 / m_Fps;

		// We want the work area duration, and durationSeconds() respects that.
		auto durationFrames = m_Animation->durationSeconds() * m_Fps;
		m_Duration = valueOrDefault(duration, durationFrames);
	}
	virtual ~RiveFrameExtractor()
	{
		delete m_AnimationInstance;
		// Deleting the file will clean up also artboard and animation.
		delete m_RiveFile;
	}
	virtual void extractFrames(int numLoops);

	float fps() const { return m_Fps; }
	int height() const { return m_Height; }
	int width() const { return m_Width; }
	void takeSnapshot(const std::string& snapshotPath) const;

protected:
	float m_IFps;
	float m_Fps;
	int m_Height;
	int m_Duration;
	int m_MinDuration;
	int m_MaxDuration;
	int m_Width;
	rive::File* m_RiveFile;
	rive::Artboard* m_Artboard;
	rive::LinearAnimation* m_Animation;
	rive::LinearAnimationInstance* m_AnimationInstance;
	sk_sp<SkImage> m_WatermarkImage;
	sk_sp<SkSurface> m_RasterSurface;
	SkCanvas* m_RasterCanvas;

	virtual void onNextFrame(int frameNumber) = 0;

	const void* getPixelAddresses() const;
	int totalFrames() const;
	rive::Artboard* getArtboard(const char* artboard_name) const;
	rive::File* getRiveFile(const char* path) const;
	rive::LinearAnimation* getAnimation(const char* artboard_name) const;
	sk_sp<SkData> getSkData(SkColor clearColor) const;
	sk_sp<SkImage> getSnapshot(SkColor clearColor) const;
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