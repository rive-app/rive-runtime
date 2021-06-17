#ifdef TESTING
#else

#include "args.hxx"
#include "recorder_arguments.hpp"
#include "render_format.hpp"
#include "extractor/extractor_factory.hpp"
#include "writer.hpp"

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
