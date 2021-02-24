#include "args.hxx"
#include "extractor.hpp"
#include "writer.hpp"
#include <fstream>
#include <string>

#ifdef TESTING
#else
int main(int argc, char* argv[])
{
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

	// Init a software scaler to do the conversion.
	SwsContext* swsCtx = sws_getContext(writer->cctx->width,
	                                    writer->cctx->height,
	                                    AV_PIX_FMT_RGBA,
	                                    writer->cctx->width,
	                                    writer->cctx->height,
	                                    AV_PIX_FMT_YUV420P,
	                                    SWS_BICUBIC,
	                                    0,
	                                    0,
	                                    0);

	sk_sp<SkSurface> rasterSurface = SkSurface::MakeRasterN32Premul(
	    writer->cctx->width, writer->cctx->height);
	SkCanvas* rasterCanvas = rasterSurface->getCanvas();

	rive::SkiaRenderer renderer(rasterCanvas);

	// We should also respect the work area here... we're just exporting the
	// entire animation for now.
	int totalFrames = extractor->animation->duration();
	float ifps = 1.0 / extractor->animation->fps();
	auto videoFrame = writer->get_av_frame();
	for (int i = 0; i < totalFrames; i++)
	{
		renderer.save();
		renderer.align(
		    rive::Fit::cover,
		    rive::Alignment::center,
		    rive::AABB(0, 0, writer->cctx->width, writer->cctx->height),
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
			    writer->cctx->width - extractor->watermarkImage->width() - 20,
			    writer->cctx->height - extractor->watermarkImage->height() - 20,
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

		// Ok some assumptions about channels here should be ok as our backing
		// Skia surface is RGBA (I think that's the N32 means). We could try to
		// optimize by having skia render RGB only since we discard the A anwyay
		// and I don't think we're compositing anything where it would matter to
		// have the alpha buffer.
		int inLinesize[1] = {4 * writer->cctx->width};
		// Get the address to the first pixel (addr8 will assert in debug mode
		// as Skia only wants you to use that with 8 bit surfaces).
		auto pixelData = pixels.addr(0, 0);
		// Run the software "scaler" really just convert from RGBA to YUV
		// here.
		sws_scale(swsCtx,
		          (const uint8_t* const*)&pixelData,
		          inLinesize,
		          0,
		          writer->cctx->height,
		          videoFrame->data,
		          videoFrame->linesize);

		// This was kind of a guess... works ok (time seems to elapse properly
		// when playing back and durations look right). PTS is still somewhat of
		// a mystery to me, I think it just needs to be monotonically
		// incrementing but there's some extra voodoo where it won't work if you
		// just use the frame number. I used to understand this stuf...
		videoFrame->pts =
		    i * writer->videoStream->time_base.den /
		    (writer->videoStream->time_base.num * extractor->animation->fps());
		int err;
		if ((err = avcodec_send_frame(writer->cctx, videoFrame)) < 0)
		{
			fprintf(stderr, "Failed to send frame %i\n", err);
			return 1;
		}

		// Send off the packet to the encoder...
		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = nullptr;
		pkt.size = 0;

		if (avcodec_receive_packet(writer->cctx, &pkt) == 0)
		{
			pkt.flags |= AV_PKT_FLAG_KEY;
			av_interleaved_write_frame(writer->ofctx, &pkt);
			av_packet_unref(&pkt);
		}
		printf(".");
		fflush(stdout);
	}
	printf(".\n");

	// Encode any delayed frames accumulated...
	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = nullptr;
	pkt.size = 0;

	for (;;)
	{
		printf("_");
		fflush(stdout);
		avcodec_send_frame(writer->cctx, nullptr);
		if (avcodec_receive_packet(writer->cctx, &pkt) == 0)
		{
			av_interleaved_write_frame(writer->ofctx, &pkt);
			av_packet_unref(&pkt);
		}
		else
		{
			break;
		}
	}
	printf(".\n");

	// Write the footer (trailer?) woo!
	av_write_trailer(writer->ofctx);
	if (!(writer->oformat->flags & AVFMT_NOFILE))
	{
		int err = avio_close(writer->ofctx->pb);
		if (err < 0)
		{
			fprintf(stderr, "Failed to close file %i\n", err);
			return 1;
		}
	}
	return 0;
}

#endif
