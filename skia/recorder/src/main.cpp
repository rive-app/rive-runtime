#include "args.hxx"
#include "extractor.hpp"
#include "writer.hpp"
#include <fstream>
#include <string>

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

	RiveFrameExtractor* extractor;
	try
	{
		extractor = new RiveFrameExtractor(args::get(source).c_str(),
		                                   args::get(artboardOption).c_str(),
		                                   args::get(animationOption).c_str(),
		                                   args::get(watermarkOption).c_str());
	}
	catch (const std::invalid_argument e)
	{
		std::cout << e.what();
		return 1;
	}

	MovieWriter* writer;
	try
	{
		writer = new MovieWriter(args::get(destination).c_str(),
		                         extractor->artboard->width(),
		                         extractor->artboard->height(),
		                         extractor->animation->fps());
	}
	catch (const std::invalid_argument e)
	{
		std::cout << e.what();
		return 1;
	}

	// We should also respect the work area here... we're just exporting the
	// entire animation for now.
	int totalFrames = extractor->animation->duration();

	writer->writeHeader();
	for (int i = 0; i < totalFrames; i++)
	{
		auto pixelData = extractor->getFrame(i);
		writer->writeFrame(i, (const uint8_t* const*)&pixelData);
	}
	writer->finalize();
}

#endif
