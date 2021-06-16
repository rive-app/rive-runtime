#include "args.hxx"
#include "recorder_arguments.hpp"
#include "render_format.hpp"
#include "video_extractor.hpp"
#include "writer.hpp"

#ifdef TESTING
#else

RiveFrameExtractor* makeExtractor(RecorderArguments& args)
{
	auto renderFormat = args.renderFormat();
	switch (renderFormat)
	{
		case RenderFormat::pngSequence:
			throw std::invalid_argument(
			    "PNG seq not supported yet! COMING SOON!");
		// return new PngExtractor();
		case RenderFormat::h264:
		default:
			return new VideoExtractor(args.source(),
			                          args.artboard(),
			                          args.animation(),
			                          args.watermark(),
			                          args.destination(),
			                          args.width(),
			                          args.height(),
			                          args.smallExtentTarget(),
			                          args.maxWidth(),
			                          args.maxHeight(),
			                          args.duration(),
			                          args.minDuration(),
			                          args.maxDuration(),
			                          args.fps(),
			                          args.bitrate());
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
