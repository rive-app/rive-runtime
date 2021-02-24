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

	sk_sp<SkSurface> rasterSurface =
	    SkSurface::MakeRasterN32Premul(extractor->width(), extractor->height());
	SkCanvas* rasterCanvas = rasterSurface->getCanvas();

	rive::SkiaRenderer renderer(rasterCanvas);

	// We should also respect the work area here... we're just exporting the
	// entire animation for now.
	int totalFrames = extractor->animation->duration();
	float ifps = 1.0 / extractor->animation->fps();

	writer->writeHeader();
	for (int i = 0; i < totalFrames; i++)
	{
		renderer.save();
		renderer.align(
		    rive::Fit::cover,
		    rive::Alignment::center,
		    rive::AABB(0, 0, extractor->width(), extractor->height()),
		    extractor->artboard->bounds());
		extractor->animation->apply(extractor->artboard, i * ifps);
		extractor->artboard->advance(0.0f);
		extractor->artboard->draw(&renderer);
		if (extractor->watermarkImage)
		{
			SkPaint watermarkPaint;
			watermarkPaint.setBlendMode(SkBlendMode::kDifference);
			rasterCanvas->drawImage(
			    extractor->watermarkImage,
			    extractor->width() - extractor->watermarkImage->width() - 20,
			    extractor->height() - extractor->watermarkImage->height() - 20,
			    &watermarkPaint);
		}
		renderer.restore();

		// After drawing the frame, grab the raw image data.
		sk_sp<SkImage> img(rasterSurface->makeImageSnapshot());
		if (!img)
		{
			return 1;
		}
		SkPixmap pixels;
		if (!img->peekPixels(&pixels))
		{
			fprintf(
			    stderr, "Failed to peek at the pixel buffer for frame %i\n", i);
			return 1;
		}

		// Get the address to the first pixel (addr8 will assert in debug mode
		// as Skia only wants you to use that with 8 bit surfaces).
		auto pixelData = pixels.addr(0, 0);

		// Run the software "scaler" really just convert from RGBA to YUV
		// here.
		writer->writeFrame(i, (const uint8_t* const*)&pixelData);
	}
	writer->finalize();
}

#endif
