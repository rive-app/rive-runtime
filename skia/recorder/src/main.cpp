#include "args.hxx"
#include "extractor.hpp"
#include "writer.hpp"
#include "recorder_arguments.hpp"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavcodec/avfft.h>

#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>

#include <libavformat/avformat.h>
#include <libavformat/avio.h>

#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/file.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
#include <libavutil/time.h>

#include <libswscale/swscale.h>
}

#ifdef TESTING
#else
int main(int argc, char* argv[])
{
	try
	{
		RecorderArguments args(argc, argv);
		RiveFrameExtractor extractor(args.source().c_str(),
		                             args.artboard().c_str(),
		                             args.animation().c_str(),
		                             args.watermark().c_str(),
		                             args.width(),
		                             args.height(),
		                             args.smallExtentTarget(),
		                             args.maxWidth(),
		                             args.maxHeight(),
		                             args.minDuration(),
		                             args.maxDuration(),
		                             args.fps());
		MovieWriter writer(args.destination().c_str(),
		                   extractor.width(),
		                   extractor.height(),
		                   extractor.fps(),
		                   args.bitrate());
		extractor.takeSnapshot(args.snapshotPath());
		extractor.extractVideo(args.numLoops(), writer);
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
