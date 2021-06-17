#ifndef _EXTRACTOR_FACTORY_HPP
#define _EXTRACTOR_FACTORY_HPP

#include "extractor/extractor.hpp"
#include "extractor/png_extractor.hpp"
#include "extractor/video_extractor.hpp"

RiveFrameExtractor* makeExtractor(RecorderArguments& args)
{
	auto renderFormat = args.renderFormat();
	auto source = args.source();
	auto artboard = args.artboard();
	auto animation = args.animation();
	auto watermark = args.watermark();
	auto destination = args.destination();
	auto width = args.width();
	auto height = args.height();
	auto smallExtentTarget = args.smallExtentTarget();
	auto maxWidth = args.maxWidth();
	auto maxHeight = args.maxHeight();
	auto duration = args.duration();
	auto minDuration = args.minDuration();
	auto maxDuration = args.maxDuration();
	auto fps = args.fps();
	auto bitrate = args.bitrate();

	switch (renderFormat)
	{
		case RenderFormat::pngSequence:
			return new PNGExtractor(source,
			                        artboard,
			                        animation,
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
			                        fps);
		case RenderFormat::h264:
		default:
			return new VideoExtractor(source,
			                          artboard,
			                          animation,
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
			                          fps,
			                          bitrate);
	}
}

#endif