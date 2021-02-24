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

public:
	rive::Artboard* artboard;
	rive::LinearAnimation* animation;
	sk_sp<SkImage> watermarkImage;
	RiveFrameExtractor(const char* path,
	                   const char* artboard_name,
	                   const char* animation_name,
	                   const char* watermark_name)
	{
		riveFile = getRiveFile(path);
		artboard = getArtboard(artboard_name);
		animation = getAnimation(animation_name);
		watermarkImage = getWaterMark(watermark_name);
	};

private:
	rive::File* riveFile;
	sk_sp<SkImage> getWaterMark(const char* watermark_name)
	{
		// Init skia surfaces to render to.
		sk_sp<SkImage> watermarkImage;
		if (watermark_name != NULL && watermark_name[0] != '\0')
		{

			if (!file_exists(watermark_name))
			{
				throw std::invalid_argument(
				    string_format("Cannot find file containing watermark at %s",
				                  watermark_name));
			}
			if (auto data = SkData::MakeFromFileName(watermark_name))
			{
				watermarkImage = SkImage::MakeFromEncoded(data);
			}
		}
		return watermarkImage;
	}

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
		//
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

class MovieWriter
{
public:
	AVCodecContext* cctx;
	AVStream* videoStream;
	AVOutputFormat* oformat;
	AVFormatContext* ofctx;
	AVCodec* codec;
	MovieWriter(const char* _destination, int _width, int _height, int _fps)
	{
		destinationFilename = _destination;
		width = _width;
		height = _height;
		fps = _fps;
		initialize();
	};

	AVFrame* get_av_frame()
	{
		// Init some ffmpeg data to hold our encoded frames (convert them to the
		// right format).
		AVFrame* videoFrame = av_frame_alloc();
		videoFrame->format = AV_PIX_FMT_YUV420P;
		videoFrame->width = cctx->width;
		videoFrame->height = cctx->height;
		int err;
		if ((err = av_frame_get_buffer(videoFrame, 32)) < 0)
		{
			throw std::invalid_argument(string_format(
			    "Failed to allocate buffer for frame with error %i\n", err));
		}
		return videoFrame;
	};

private:
	const char* destinationFilename;

	// Ok we should have some more optional params for these:
	int width, height, fps;
	int bitrate = 400;

	void initialize()
	{
		// if init fails all this stuff needs cleaning up?

		// Try to guess the output format from the name.
		oformat = av_guess_format(nullptr, destinationFilename, nullptr);
		if (!oformat)
		{
			throw std::invalid_argument(
			    string_format("Failed to determine output format for %s\n.",
			                  destinationFilename));
		}

		// Get a context for the format to work with (I guess the OutputFormat
		// is sort of the blueprint, and this is the instance for this specific
		// run of it).
		ofctx = nullptr;
		// TODO: there's probably cleanup to do here.
		if (avformat_alloc_output_context2(
		        &ofctx, oformat, nullptr, destinationFilename) < 0)
		{
			throw std::invalid_argument(
			    string_format("Failed to allocate output context %s\n.",
			                  destinationFilename));
		}
		// Check that we have the necessary codec for the format we want to
		// encode (I think most formats can have multiple codecs so this
		// probably tries to guess the best default available one).
		codec = avcodec_find_encoder(oformat->video_codec);
		if (!codec)
		{
			throw std::invalid_argument(string_format(
			    "Failed to find codec for %s\n.", destinationFilename));
		}

		// Allocate the stream we're going to be writing to.
		videoStream = avformat_new_stream(ofctx, codec);
		if (!videoStream)
		{
			throw std::invalid_argument(string_format(
			    "Failed to create a stream for %s\n.", destinationFilename));
		}

		// Similar to AVOutputFormat and AVFormatContext, the codec needs an
		// instance/"context" to store data specific to this run.
		cctx = avcodec_alloc_context3(codec);
		if (!cctx)
		{
			throw std::invalid_argument(
			    string_format("Failed to allocate codec context for "
			                  "%s\n.",
			                  destinationFilename));
		}

		videoStream->codecpar->codec_id = oformat->video_codec;
		videoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
		videoStream->codecpar->width = width;
		videoStream->codecpar->height = height;
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

		// OK! Finally set the parameters on the stream from the codec context
		// we just fucked with.
		avcodec_parameters_from_context(videoStream->codecpar, cctx);

		if (avcodec_open2(cctx, codec, NULL) < 0)
		{
			throw std::invalid_argument(string_format(
			    "Failed to open codec %i\n", destinationFilename));
		}

		// Finally open the file! Interesting step here, I guess some files can
		// just record to memory or something, so they don't actually need a
		// file to open io.
		if (!(oformat->flags & AVFMT_NOFILE))
		{
			if (avio_open(&ofctx->pb, destinationFilename, AVIO_FLAG_WRITE) < 0)
			{
				throw std::invalid_argument(
				    string_format("Failed to open file %s with error %i\n",
				                  destinationFilename));
			}
		}

		// Header time...
		if (avformat_write_header(ofctx, NULL) < 0)
		{
			throw std::invalid_argument(string_format(
			    "Failed to write header %i\n", destinationFilename));
		}

		// Write the format into the header...
		av_dump_format(ofctx, 0, destinationFilename, 1);
	}
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
