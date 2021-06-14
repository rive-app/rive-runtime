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
			return new VideoExtractor(args);
		default:
			return new VideoExtractor(args);
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
