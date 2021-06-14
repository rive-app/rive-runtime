#include "args.hxx"
#include "recorder_arguments.hpp"
#include "video_extractor.hpp"
#include "writer.hpp"

#ifdef TESTING
#else

RiveFrameExtractor* makeExtractor(RecorderArguments& args)
{
	auto renderType = args.renderType();
	switch (renderType)
	{
		case ExtractorType::pngSequence:
			throw std::invalid_argument(
			    "PNG seq not supported yet! COMING SOON!");
		// return new PngExtractor();
		case ExtractorType::h264:
		default:
			return new VideoExtractor(args.source().c_str(),
			                          args.artboard().c_str(),
			                          args.animation().c_str(),
			                          args.watermark().c_str(),
			                          args.destination().c_str(),
			                          args.width(),
			                          args.height(),
			                          args.smallExtentTarget(),
			                          args.maxWidth(),
			                          args.maxHeight(),
			                          args.minDuration(),
			                          args.maxDuration(),
			                          args.fps(),
			                          args.bitrate());
	}
}

int main(int argc, char* argv[])
{
	try
	{
		RecorderArguments args(argc, argv);
		auto extractor = makeExtractor(args);
		extractor->takeSnapshot(args.snapshotPath());
		extractor->extractFrames(args.numLoops());
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
}

#endif
