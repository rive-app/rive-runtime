#include "extractor/video_extractor.hpp"

VideoExtractor::VideoExtractor(const std::string& path,
                               const std::string& artboardName,
                               const std::string& animationName,
                               const std::string& watermark,
                               const std::string& destination,
                               int width,
                               int height,
                               int smallExtentTarget,
                               int maxWidth,
                               int maxHeight,
                               int duration,
                               int minDuration,
                               int maxDuration,
                               int fps,
                               int bitrate)
{
	m_MinDuration = minDuration;
	m_MaxDuration = maxDuration;
	m_RiveFile = getRiveFile(path.c_str());
	m_Artboard = getArtboard(artboardName.c_str());
	m_Animation = getAnimation(animationName.c_str());
	m_Animation_instance = new rive::LinearAnimationInstance(m_Animation);
	m_WatermarkImage = getWatermark(watermark.c_str());
	initializeDimensions(width, height, smallExtentTarget, maxWidth, maxHeight);
	m_RasterSurface = SkSurface::MakeRaster(SkImageInfo::Make(
	    m_Width, m_Height, kRGBA_8888_SkColorType, kPremul_SkAlphaType));
	m_RasterCanvas = m_RasterSurface->getCanvas();
	m_Fps = valueOrDefault(fps, m_Animation->fps());
	m_IFps = 1.0 / m_Fps;

	// We want the work area duration, and durationSeconds() respects that.
	auto durationFrames = m_Animation->durationSeconds() * m_Fps;
	m_Duration = valueOrDefault(duration, durationFrames);

	m_movieWriter =
	    new MovieWriter(destination, m_Width, m_Height, m_Fps, bitrate);
}

VideoExtractor::~VideoExtractor()
{
	if (m_movieWriter)
	{
		delete m_movieWriter;
	}
	// TODO: move these in parent class.
	if (m_Animation_instance != nullptr)
	{
		delete m_Animation_instance;
	}
	if (m_RiveFile != nullptr)
	{
		delete m_RiveFile;
	}
}

void VideoExtractor::extractFrames(int numLoops) const
{
	m_movieWriter->writeHeader();
	RiveFrameExtractor::extractFrames(numLoops);
	m_movieWriter->finalize();
}

void VideoExtractor::onNextFrame(int frameNumber) const
{
	auto pixelData = this->getPixelAddresses();
	m_movieWriter->writeFrame(frameNumber, (const uint8_t* const*)&pixelData);
}