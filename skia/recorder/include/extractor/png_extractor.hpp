#ifndef _PNG_EXTRACTOR_HPP
#define _PNG_EXTRACTOR_HPP

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "miniz.h"
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
	             int fps = 0);
	virtual ~PNGExtractor() {}

	void extractFrames(int numLoops) override;
	void onNextFrame(int frameNumber) override;

	static int numDigits(unsigned number)
	{
		int digits = 0;
		unsigned temp = number;
		while (temp)
		{
			temp /= 10;
			digits++;
		}

		return digits;
	}

private:
	std::string m_DestinationPath;
	unsigned m_Digits;

	std::string zeroPadded(unsigned frameNumber);
};

#endif