#ifndef VIDEOEXTRACTOR_HPP
#define VIDEOEXTRACTOR_HPP

#include "extractor/extractor.hpp"
#include "recorder_arguments.hpp"
#include "writer.hpp"

class VideoExtractor : public RiveFrameExtractor
{
public:
	VideoExtractor(const std::string& path,
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
	               int fps = 0,
	               int bitrate = 0);
	virtual ~VideoExtractor();

	void extractFrames(int numLoops) override;

protected:
	void onNextFrame(int frameNumber) override;

private:
	MovieWriter* m_movieWriter;
};

#endif