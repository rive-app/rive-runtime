#ifndef VIDEOEXTRACTOR_HPP
#define VIDEOEXTRACTOR_HPP

#include "extractor.hpp"
#include "recorder_arguments.hpp"
#include "writer.hpp"

class VideoExtractor : public RiveFrameExtractor
{
public:
	VideoExtractor(std::string path,
	               std::string artboardName,
	               std::string animationName,
	               std::string watermark,
	               std::string destination,
	               int width = 0,
	               int height = 0,
	               int smallExtentTarget = 0,
	               int maxWidth = 0,
	               int maxHeight = 0,
	               int minDuration = 0,
	               int maxDuration = 0,
	               int fps = 0,
	               int bitrate = 0)
	{
		m_MinDuration = minDuration;
		m_MaxDuration = maxDuration;
		m_RiveFile = getRiveFile(path.c_str());
		m_Artboard = getArtboard(artboardName.c_str());
		m_Animation = getAnimation(animationName.c_str());
		m_Animation_instance = new rive::LinearAnimationInstance(m_Animation);
		m_WatermarkImage = getWatermark(watermark.c_str());
		initializeDimensions(
		    width, height, smallExtentTarget, maxWidth, maxHeight);
		m_RasterSurface = SkSurface::MakeRaster(SkImageInfo::Make(
		    m_Width, m_Height, kRGBA_8888_SkColorType, kPremul_SkAlphaType));
		m_RasterCanvas = m_RasterSurface->getCanvas();
		m_Fps = valueOrDefault(fps, m_Animation->fps());
		m_IFps = 1.0 / m_Fps;

		m_movieWriter =
		    new MovieWriter(destination, m_Width, m_Height, m_Fps, bitrate);
	}

	~VideoExtractor()
	{
		if (m_movieWriter != nullptr)
			delete m_movieWriter;
	}

	void extractFrames(int numLoops) const override
	{
		m_movieWriter->writeHeader();
		RiveFrameExtractor::extractFrames(numLoops);
		m_movieWriter->finalize();
	}

protected:
	void onNextFrame(int frameNumber) const override
	{
		auto pixelData = this->getPixelAddresses();
		m_movieWriter->writeFrame(frameNumber,
		                          (const uint8_t* const*)&pixelData);
	}

private:
	MovieWriter* m_movieWriter;
};

#endif