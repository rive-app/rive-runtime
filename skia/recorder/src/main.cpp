#include "main.hpp"
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

std::string getFileName(const char* path)
{
	std::string str(path);

	const size_t from = str.find_last_of("\\/");
	const size_t to = str.find_last_of(".");
	return str.substr(from + 1, to - from - 1);
}

template <typename... Args>
std::string string_format(const std::string& format, Args... args)
{
	int size = snprintf(nullptr, 0, format.c_str(), args...) +
	           1; // Extra space for '\0'
	if (size <= 0)
	{
		throw std::runtime_error("Error during formatting.");
	}
	std::unique_ptr<char[]> buf(new char[size]);
	snprintf(buf.get(), size, format.c_str(), args...);
	return std::string(buf.get(),
	                   buf.get() + size - 1); // We don't want the '\0' inside
}

// QUESTION: inline looks fun. so this copy pastes the code around i guess, but
// its considered faster than referencing code?
inline bool file_exists(const std::string& name)
{
	// https://stackoverflow.com/questions/12774207/fastest-way-to-check-if-a-file-exist-using-standard-c-c11-c
	struct stat buffer;
	return (stat(name.c_str(), &buffer) == 0);
}

class RiveFrameExtractor
{
	rive::File* riveFile;

public:
	rive::Artboard* artboard;
	rive::LinearAnimation* animation;
	RiveFrameExtractor(const char* path,
	                   const char* artboard_name,
	                   const char* animation_name)
	{
		riveFile = getRiveFile(path);
		artboard = getArtboard(artboard_name);
		animation = getAnimation(animation_name);
	};

private:
	rive::File* getRiveFile(const char* path)
	{
		FILE* fp = fopen(path, "r");

		if (fp == nullptr)
		{
			throw std::invalid_argument(
			    string_format("Failed to open file %s", path));
		}
		fseek(fp, 0, SEEK_END);
		auto length = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		// TODO: need to clean this up? how?!
		uint8_t* bytes = new uint8_t[length];

		if (fread(bytes, 1, length, fp) != length)
		{
			throw std::invalid_argument(
			    string_format("Failed to read file into bytes array %s", path));
		}

		auto reader = rive::BinaryReader(bytes, length);
		rive::File* file = nullptr;
		auto result = rive::File::import(reader, &file);
		if (result != rive::ImportResult::success)
		{
			throw std::invalid_argument(
			    string_format("Failed to read bytes into Rive file %s", path));
		}
		return file;
	}

	rive::Artboard* getArtboard(const char* artboard_name)
	{
		// Figure out which artboard to use.
		rive::Artboard* artboard;

		// QUESTION: better to keep this logic in the main? or pass the flag in
		// here, so we can bool check if the flag is set or not? what happens if
		// we try to target the artboard '' otherwise?
		if (artboard_name != NULL && artboard_name[0] != '\0')
		{
			if ((artboard = riveFile->artboard(artboard_name)) == nullptr)
			{
				throw std::invalid_argument(
				    string_format("File doesn't contain an artboard named %s.",
				                  artboard_name));
			}
		}
		else
		{
			artboard = riveFile->artboard();
			if (artboard == nullptr)
			{
				throw std::invalid_argument(
				    string_format("File doesn't contain an default artboard.",
				                  artboard_name));
			}
		}
		return artboard;
	}

	rive::LinearAnimation* getAnimation(const char* animation_name)
	{
		// Figure out which animation to use.
		rive::LinearAnimation* animation;
		if (animation_name != NULL && animation_name[0] != '\0')
		{
			if ((animation = artboard->animation<rive::LinearAnimation>(
			         animation_name)) == nullptr)
			{

				fprintf(stderr,
				        "Artboard doesn't contain an animation named %s.\n",
				        animation_name);
			}
		}
		else
		{
			animation = artboard->firstAnimation<rive::LinearAnimation>();
			if (animation == nullptr)
			{
				throw std::invalid_argument(string_format(
				    "Artboard doesn't contain a default animation."));
			}
		}
		return animation;
	};
};

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
		                                   args::get(animationOption).c_str());
	}
	catch (const std::invalid_argument e)
	{
		std::cout << e.what();
		return 1;
	}

	// Init skia surfaces to render to.
	sk_sp<SkImage> watermarkImage;
	if (watermarkOption)
	{
		auto watermarkFilename = args::get(watermarkOption);
		if (!file_exists(watermarkFilename))
		{
			fprintf(stderr,
			        "Watermark file cannot be found at %s.\n",
			        watermarkFilename.c_str());
			return 1;
		}
		if (auto data = SkData::MakeFromFileName(watermarkFilename.c_str()))
		{
			watermarkImage = SkImage::MakeFromEncoded(data);
		}
	}

	// Cool, file's sane, let's start initializing the video recorder.
	auto destinationFilename = args::get(destination);

	// Note because this is one shot app we don't take care of cleaning up
	// resources. If this needs to be a longer lived worker app, we should
	// really build a pool of Recorder objects that handle this nicely.

	// Try to guess the output format from the name.
	AVOutputFormat* oformat;
	if (!(oformat =
	          av_guess_format(nullptr, destinationFilename.c_str(), nullptr)))
	{
		fprintf(stderr,
		        "Failed to determine output format for %s\n.",
		        destinationFilename.c_str());
		return 1;
	}

	int err;

	// Get a context for the format to work with (I guess the OutputFormat
	// is sort of the blueprint, and this is the instance for this specific
	// run of it).
	AVFormatContext* ofctx = nullptr;
	if ((err = avformat_alloc_output_context2(
	               &ofctx, oformat, nullptr, destinationFilename.c_str()) < 0))
	{
		fprintf(stderr,
		        "Failed to allocate output context %s\n.",
		        destinationFilename.c_str());
		// This is where something in a longer lived app would cleanup the
		// format previously allocated.
		return 1;
	}

	// Check that we have the necessary codec for the format we want to
	// encode (I think most formats can have multiple codecs so this
	// probably tries to guess the best default available one).
	AVCodec* codec;
	if (!(codec = avcodec_find_encoder(oformat->video_codec)))
	{
		fprintf(stderr,
		        "Failed to find codec for %s\n.",
		        destinationFilename.c_str());
		// This is where something in a longer lived app would cleanup the
		// oformat and ofctx previously allocated.
		return 1;
	}

	// Allocate the stream we're going to be writing to.
	AVStream* videoStream;
	if (!(videoStream = avformat_new_stream(ofctx, codec)))
	{
		fprintf(stderr,
		        "Failed to create a stream for %s\n.",
		        destinationFilename.c_str());

		// This is where something in a longer lived app would cleanup the
		// oformat, ofctx, and codec previously allocated. I'm going to stop
		// doing these, you get it.
		return 1;
	}

	// Similar to AVOutputFormat and AVFormatContext, the codec needs an
	// instance/"context" to store data specific to this run.
	AVCodecContext* cctx = nullptr;
	if (!(cctx = avcodec_alloc_context3(codec)))
	{
		fprintf(stderr,
		        "Failed to allocate codec context for %s\n.",
		        destinationFilename.c_str());
		// Cleanup you swine...
		return 1;
	}

	// Coooool we made it this far! We can now start initializing the video
	// stream to use the right codec we figured out.

	// Ok we should have some more optional params for these:
	int bitrate = 400;
	int fps = extractor->animation->fps();
	int videoWidth = (int)extractor->artboard->width();
	int videoHeight = (int)extractor->artboard->height();

	videoStream->codecpar->codec_id = oformat->video_codec;
	videoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
	videoStream->codecpar->width = videoWidth;
	videoStream->codecpar->height = videoHeight;
	videoStream->codecpar->format = AV_PIX_FMT_YUV420P;
	videoStream->codecpar->bit_rate = bitrate * 1000;
	videoStream->time_base = {1, fps};

	// Yeah so these are just some numbers that work, we'll probably want to
	// fine tune these...
	avcodec_parameters_to_context(cctx, videoStream->codecpar);
	cctx->time_base = {1, fps};
	cctx->max_b_frames = 2;
	cctx->gop_size = 12;
	if (videoStream->codecpar->codec_id == AV_CODEC_ID_H264)
	{
		// Set the H264 preset to shite but fast, I guess?
		av_opt_set(cctx, "preset", "ultrafast", 0);
	}
	else if (videoStream->codecpar->codec_id == AV_CODEC_ID_H265)
	{
		// More beauty
		av_opt_set(cctx, "preset", "ultrafast", 0);
	}

	// OK! Finally set the parameters on the stream from the codec context we
	// just fucked with.
	avcodec_parameters_from_context(videoStream->codecpar, cctx);

	if ((err = avcodec_open2(cctx, codec, NULL)) < 0)
	{
		fprintf(stderr, "Failed to open codec %i\n", err);
		// Cleanup crew...
		return 1;
	}

	// Finally open the file! Interesting step here, I guess some files can just
	// record to memory or something, so they don't actually need a file to
	// open io.
	if (!(oformat->flags & AVFMT_NOFILE))
	{
		if ((err = avio_open(
		         &ofctx->pb, destinationFilename.c_str(), AVIO_FLAG_WRITE)) < 0)
		{
			fprintf(stderr,
			        "Failed to open file %s with error %i\n",
			        destinationFilename.c_str(),
			        err);
			// You know what to do...
			return 1;
		}
	}

	// Header time...
	if ((err = avformat_write_header(ofctx, NULL)) < 0)
	{
		fprintf(stderr, "Failed to write header %i\n", err);
		return 1;
	}

	// Write the format into the header...
	av_dump_format(ofctx, 0, destinationFilename.c_str(), 1);

	// ALRIGHT! At this point we're ready to start adding frames!

	// Init some ffmpeg data to hold our encoded frames (convert them to the
	// right format).
	AVFrame* videoFrame = av_frame_alloc();
	videoFrame->format = AV_PIX_FMT_YUV420P;
	videoFrame->width = cctx->width;
	videoFrame->height = cctx->height;
	if ((err = av_frame_get_buffer(videoFrame, 32)) < 0)
	{
		fprintf(
		    stderr, "Failed to allocate buffer for frame with error %i\n", err);
		return 1;
	}

	// Init a software scaler to do the conversion.
	SwsContext* swsCtx = sws_getContext(cctx->width,
	                                    cctx->height,
	                                    AV_PIX_FMT_RGBA,
	                                    cctx->width,
	                                    cctx->height,
	                                    AV_PIX_FMT_YUV420P,
	                                    SWS_BICUBIC,
	                                    0,
	                                    0,
	                                    0);

	sk_sp<SkSurface> rasterSurface =
	    SkSurface::MakeRasterN32Premul(cctx->width, cctx->height);
	SkCanvas* rasterCanvas = rasterSurface->getCanvas();

	rive::SkiaRenderer renderer(rasterCanvas);

	// We should also respect the work area here... we're just exporting the
	// entire animation for now.
	int totalFrames = extractor->animation->duration();
	float ifps = 1.0 / fps;
	for (int i = 0; i < totalFrames; i++)
	{
		renderer.save();
		renderer.align(rive::Fit::cover,
		               rive::Alignment::center,
		               rive::AABB(0, 0, cctx->width, cctx->height),
		               extractor->artboard->bounds());
		extractor->animation->apply(extractor->artboard, i * ifps);
		extractor->artboard->advance(0.0f);
		extractor->artboard->draw(&renderer);
		if (watermarkImage)
		{
			SkPaint watermarkPaint;
			watermarkPaint.setBlendMode(SkBlendMode::kDifference);
			rasterCanvas->drawImage(watermarkImage,
			                        cctx->width - watermarkImage->width() - 20,
			                        cctx->height - watermarkImage->height() -
			                            20,
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
		int inLinesize[1] = {4 * cctx->width};
		// Get the address to the first pixel (addr8 will assert in debug mode
		// as Skia only wants you to use that with 8 bit surfaces).
		auto pixelData = pixels.addr(0, 0);
		// Run the software "scaler" really just convert from RGBA to YUV
		// here.
		sws_scale(swsCtx,
		          (const uint8_t* const*)&pixelData,
		          inLinesize,
		          0,
		          cctx->height,
		          videoFrame->data,
		          videoFrame->linesize);

		// This was kind of a guess... works ok (time seems to elapse properly
		// when playing back and durations look right). PTS is still somewhat of
		// a mystery to me, I think it just needs to be monotonically
		// incrementing but there's some extra voodoo where it won't work if you
		// just use the frame number. I used to understand this stuf...
		videoFrame->pts =
		    i * videoStream->time_base.den / (videoStream->time_base.num * fps);
		if ((err = avcodec_send_frame(cctx, videoFrame)) < 0)
		{
			fprintf(stderr, "Failed to send frame %i\n", err);
			return 1;
		}

		// Send off the packet to the encoder...
		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = nullptr;
		pkt.size = 0;

		if (avcodec_receive_packet(cctx, &pkt) == 0)
		{
			pkt.flags |= AV_PKT_FLAG_KEY;
			av_interleaved_write_frame(ofctx, &pkt);
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
		avcodec_send_frame(cctx, nullptr);
		if (avcodec_receive_packet(cctx, &pkt) == 0)
		{
			av_interleaved_write_frame(ofctx, &pkt);
			av_packet_unref(&pkt);
		}
		else
		{
			break;
		}
	}
	printf(".\n");

	// Write the footer (trailer?) woo!
	av_write_trailer(ofctx);
	if (!(oformat->flags & AVFMT_NOFILE))
	{
		int err = avio_close(ofctx->pb);
		if (err < 0)
		{
			fprintf(stderr, "Failed to close file %i\n", err);
			return 1;
		}
	}
	return 0;
}

#endif
