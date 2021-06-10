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
#include "file.hpp"
#include "skia_renderer.hpp"
#include "util.hpp"
#include "util.hxx"
#include "writer.hpp"

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
	float fps() const;

	void takeSnapshot(const std::string& snapshotPath) const
	{
		if (snapshotPath.empty())
		{
			return;
		}
		this->restart();

		this->advanceFrame();
		SkFILEWStream out(snapshotPath.c_str());
		auto png = this->getSkData();
		(void)out.write(png->data(), png->size());

		this->restart();
	}

	void extractVideo(int numLoops, MovieWriter& writer) const
	{
		writer.writeHeader();
		int totalFrames = this->totalFrames();
		for (int loops = 0; loops < numLoops; loops++)
		{
			// Reset the animation time to the start
			this->restart();
			for (int i = 0; i < totalFrames; i++)
			{
				this->advanceFrame();
				auto pixelData = this->getPixelAddresses();
				int frameNumber = loops * totalFrames + i;
				writer.writeFrame(frameNumber,
				                   (const uint8_t* const*)&pixelData);
			}
		}
		writer.finalize();
	}

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

	int totalFrames() const;
	void advanceFrame() const;
	void restart() const;
	const void* getPixelAddresses() const;
	sk_sp<SkData> getSkData() const;

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