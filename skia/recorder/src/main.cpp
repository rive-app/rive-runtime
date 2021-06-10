#include "args.hxx"
#include "extractor.hpp"
#include "writer.hpp"

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
	// PARSE INPUT
	args::ArgumentParser parser(
	    "Record playback of a Rive file as a movie, gif, etc (eventually "
	    "should support image sequences saved in a zip/archive too).",
	    "Experimental....");
	args::HelpFlag help(
	    parser, "help", "Display this help menu", {'h', "help"});
	args::Group required(
	    parser, "required arguments:", args::Group::Validators::All);
	args::Group optional(
	    parser, "optional arguments:", args::Group::Validators::DontCare);

	args::ValueFlag<std::string> source(
	    required, "path", "source filename", {'s', "source"});
	args::ValueFlag<std::string> destination(
	    required, "path", "destination filename", {'d', "destination"});
	args::ValueFlag<std::string> animationOption(
	    optional,
	    "name",
	    "animation to be played, determines the numbers of frames recorded",
	    {'a', "animation"});
	args::ValueFlag<std::string> artboardOption(
	    optional, "name", "artboard to draw from", {'t', "artboard"});
	args::ValueFlag<std::string> watermarkOption(
	    optional, "path", "watermark filename", {'w', "watermark"});

	args::ValueFlag<int> width(
	    optional, "number", "container width", {'W', "width"}, 0);

	args::ValueFlag<int> height(
	    optional, "number", "container height", {'H', "height"}, 0);

	args::ValueFlag<int> small_extent_target(
	    optional,
	    "number",
	    "target size for smaller edge of container",
	    {"small-extent-target"},
	    0);

	args::ValueFlag<int> max_width(
	    optional, "number", "maximum container width", {"max-width"}, 0);

	args::ValueFlag<int> max_height(
	    optional, "number", "maximum container height", {"max-height"}, 0);

	args::ValueFlag<int> min_duration(
	    optional, "number", "minimum duration in seconds", {"min-duration"}, 0);

	args::ValueFlag<int> max_duration(
	    optional, "number", "maximum duration in seconds", {"max-duration"}, 0);

	args::ValueFlag<int> num_loops(
	    optional,
	    "number",
	    "number of times this video should be looped for",
	    {"num-loops"},
	    1);

	// 0 will use VBR. By setting a bitrate value, users enforce CBR.
	args::ValueFlag<int> bitrate(
	    optional, "number", "bitrate in kbps", {"bitrate"}, 0);

	args::ValueFlag<std::string> snapshot_path(
	    optional, "path", "destination image filename", {"snapshot-path"});

	args::ValueFlag<float> fps(
	    required, "number", "frame rate", {"f", "fps"}, 60.0);

	args::CompletionFlag completion(parser, {"complete"});

	try
	{
		parser.ParseCLI(argc, argv);
	}
	catch (const args::Completion& e)
	{
		std::cout << e.what();
		return 0;
	}
	catch (const args::Help&)
	{
		std::cout << parser;
		return 0;
	}
	catch (const args::ParseError& e)
	{
		std::cerr << e.what() << std::endl;
		std::cerr << parser;
		return 1;
	}
	catch (args::ValidationError e)
	{
		std::cerr << e.what() << std::endl;
		std::cerr << parser;
		return 1;
	}

	try
	{
		RiveFrameExtractor extractor(args::get(source).c_str(),
		                             args::get(artboardOption).c_str(),
		                             args::get(animationOption).c_str(),
		                             args::get(watermarkOption).c_str(),
		                             args::get(width),
		                             args::get(height),
		                             args::get(small_extent_target),
		                             args::get(max_width),
		                             args::get(max_height),
		                             args::get(min_duration),
		                             args::get(max_duration),
		                             args::get(fps));
		MovieWriter writer(args::get(destination).c_str(),
		                   extractor.width(),
		                   extractor.height(),
		                   extractor.fps(),
		                   args::get(bitrate));
		extractor.takeSnapshot(args::get(snapshot_path));
		extractor.extractVideo(args::get(num_loops), writer);
	}
	catch (const std::invalid_argument e)
	{
		std::cout << e.what();
		return 1;
	}
}

#endif
