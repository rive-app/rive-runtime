#include <sstream>
#include <iomanip>
#include "extractor/png_extractor.hpp"

PNGExtractor::PNGExtractor(const std::string& path,
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
                           int fps) :
    RiveFrameExtractor(path,
                       artboardName,
                       animationName,
                       watermark,
                       destination,
                       width,
                       height,
                       smallExtentTarget,
                       maxWidth,
                       maxHeight,
                       duration,
                       minDuration,
                       maxDuration,
                       fps),
    m_DestinationPath(destination),
    m_Digits(0)
{
}

std::string PNGExtractor::zeroPadded(unsigned frameNumber)
{
	std::ostringstream stream;
	stream << std::setw(m_Digits) << std::setfill('0') << frameNumber;
	return stream.str();
}

void PNGExtractor::onNextFrame(int frameNumber)
{
	// Make sure we have a transparent background.
	sk_sp<SkData> png = this->getSkData(SK_ColorTRANSPARENT);
	auto buffer = png->data();
	auto size = png->size();
	auto pngName = zeroPadded(frameNumber) + ".png";
	mz_zip_add_mem_to_archive_file_in_place(m_DestinationPath.c_str(),
	                                        pngName.c_str(),
	                                        buffer,
	                                        size,
	                                        0,
	                                        0,
	                                        MZ_BEST_COMPRESSION);
}

void PNGExtractor::extractFrames(int numLoops)
{
	unsigned totalFrames = (numLoops * this->totalFrames()) - 1;
	m_Digits = numDigits(totalFrames);
	RiveFrameExtractor::extractFrames(numLoops);
}