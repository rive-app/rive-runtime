#ifndef _PNG_EXTRACTOR_HPP
#define _PNG_EXTRACTOR_HPP

#include "archive.hpp"
#include "extractor/extractor.hpp"

class PNGExtractor : public RiveFrameExtractor
{
public:
	PNGExtractor(const std::string& path,
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
	    m_Archive(Archive(destination))
	{
	}
	virtual ~PNGExtractor() {}

	void extractFrames(int numLoops)
	{
		RiveFrameExtractor::extractFrames(numLoops);
		// Write it out to disk.
		m_Archive.finalize();
	}

	void onNextFrame(int frameNumber) const
	{
		sk_sp<SkData> png = this->getSkData();
		auto buffer = png->data();
		auto size = png->size();
		m_Archive.addBuffer(std::to_string(frameNumber) + ".png", buffer, size);
	}

private:
	Archive m_Archive;
};

#endif