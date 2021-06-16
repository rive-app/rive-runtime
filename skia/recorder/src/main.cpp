#include "args.hxx"
#include "recorder_arguments.hpp"
#include "render_format.hpp"
#include "extractor/video_extractor.hpp"
#include "extractor/png_extractor.hpp"
#include "writer.hpp"

#ifdef TESTING
#else

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

int main(int argc, const char* argv[])
{
	try
	{
		RecorderArguments args(argc, argv);
		RiveFrameExtractor* extractor = makeExtractor(args);
		extractor->takeSnapshot(args.snapshotPath());
		extractor->extractFrames(args.numLoops());
		delete extractor;
	}
	catch (const args::Completion& e)
	{
		return 0;
	}
	catch (const args::Help&)
	{
		return 0;
	}
	catch (const args::ParseError& e)
	{
		return 1;
	}
	catch (args::ValidationError e)
	{
		return 1;
	}

	return 0;
}

#endif
