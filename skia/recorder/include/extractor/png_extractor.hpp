#ifndef _PNG_EXTRACTOR_HPP
#define _PNG_EXTRACTOR_HPP

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "miniz.h"
#include "extractor/extractor.hpp"
#include <fstream>
#include <cstdio>
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
	    m_DestinationPath(destination)
	{
	}
	virtual ~PNGExtractor() {}

	void onNextFrame(int frameNumber)
	{
		// Make sure we have a transparent background.
		sk_sp<SkData> png = this->getSkData(SK_ColorTRANSPARENT);
		auto buffer = png->data();
		auto size = png->size();
		auto pngName = std::to_string(frameNumber) + ".png";
		mz_zip_add_mem_to_archive_file_in_place(m_DestinationPath.c_str(),
		                                        pngName.c_str(),
		                                        buffer,
		                                        size,
		                                        0,
		                                        0,
		                                        MZ_BEST_COMPRESSION);
	}

private:
	std::string m_DestinationPath;
};

#endif