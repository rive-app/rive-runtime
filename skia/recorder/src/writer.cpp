#include "writer.hpp"

MovieWriter::MovieWriter(const char* _destination,
                         int _width,
                         int _height,
                         int _fps,
                         const char* _snapshot,
                         int _bitrate)
{
	destinationPath = _destination;
	width = _width;
	height = _height;
	fps = _fps;
	bitrate = _bitrate;
	snapshotPath = _snapshot;
	initialize();
};

void MovieWriter::initialize()
{
	// if init fails all this stuff needs cleaning up?

	// Try to guess the output format from the name.
	oformat = av_guess_format(nullptr, destinationPath, nullptr);
	if (!oformat)
	{
		throw std::invalid_argument(string_format(
		    "Failed to determine output format for %s\n.", destinationPath));
	}

	// Get a context for the format to work with (I guess the OutputFormat
	// is sort of the blueprint, and this is the instance for this specific
	// run of it).
	ofctx = nullptr;
	// TODO: there's probably cleanup to do here.
	if (avformat_alloc_output_context2(
	        &ofctx, oformat, nullptr, destinationPath) < 0)
	{
		throw std::invalid_argument(string_format(
		    "Failed to allocate output context %s\n.", destinationPath));
	}
	// Check that we have the necessary codec for the format we want to
	// encode (I think most formats can have multiple codecs so this
	// probably tries to guess the best default available one).
	codec = avcodec_find_encoder(oformat->video_codec);
	if (!codec)
	{
		throw std::invalid_argument(
		    string_format("Failed to find codec for %s\n.", destinationPath));
	}

	// Allocate the stream we're going to be writing to.
	videoStream = avformat_new_stream(ofctx, codec);
	if (!videoStream)
	{
		throw std::invalid_argument(string_format(
		    "Failed to create a stream for %s\n.", destinationPath));
	}

	// Similar to AVOutputFormat and AVFormatContext, the codec needs an
	// instance/"context" to store data specific to this run.
	cctx = avcodec_alloc_context3(codec);
	if (!cctx)
	{
		throw std::invalid_argument(
		    string_format("Failed to allocate codec context for "
		                  "%s\n.",
		                  destinationPath));
	}

	// default to our friend yuv, mp4 is basically locked onto this.
	pixel_format = AV_PIX_FMT_YUV420P;
	if (oformat->video_codec == AV_CODEC_ID_GIF)
	{
		// for some reason we dont get anything actually animating here...
		// we're getting the same frame over and over
		pixel_format = AV_PIX_FMT_RGB8;

		// I think these are the formats that should work here
		// https://ffmpeg.org/doxygen/trunk/libavcodec_2gif_8c.html
		//     .pix_fmts       = (const enum AVPixelFormat[]){
		//     AV_PIX_FMT_RGB8, AV_PIX_FMT_BGR8, AV_PIX_FMT_RGB4_BYTE,
		//     AV_PIX_FMT_BGR4_BYTE, AV_PIX_FMT_GRAY8, AV_PIX_FMT_PAL8,
		//     AV_PIX_FMT_NONE
		// },
	}

	videoStream->codecpar->codec_id = oformat->video_codec;
	videoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
	videoStream->codecpar->width = width;
	videoStream->codecpar->height = height;
	videoStream->codecpar->format = pixel_format;
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
		throw std::invalid_argument(
		    string_format("Failed to open codec %i\n", destinationPath));
	}
	// initialise_av_frame();
}

void MovieWriter::initialise_av_frame()
{
	// Init some ffmpeg data to hold our encoded frames (convert them to the
	// right format).
	videoFrame = av_frame_alloc();
	videoFrame->format = pixel_format;
	videoFrame->width = width;
	videoFrame->height = height;
	int err;
	if ((err = av_frame_get_buffer(videoFrame, 32)) < 0)
	{
		throw std::invalid_argument(string_format(
		    "Failed to allocate buffer for frame with error %i\n", err));
	}
};

void MovieWriter::writeHeader()
{
	// Finally open the file! Interesting step here, I guess some files can
	// just record to memory or something, so they don't actually need a
	// file to open io.
	if (!(oformat->flags & AVFMT_NOFILE))
	{
		int err;
		if ((err = avio_open(&ofctx->pb, destinationPath, AVIO_FLAG_WRITE)) < 0)
		{
			throw std::invalid_argument(
			    string_format("Failed to open file %s with error %i\n", err));
		}
	}

	// Header time...
	if (avformat_write_header(ofctx, NULL) < 0)
	{
		throw std::invalid_argument(
		    string_format("Failed to write header %i\n", destinationPath));
	}

	// Write the format into the header...
	av_dump_format(ofctx, 0, destinationPath, 1);

	// Init a software scaler to do the conversion.
	swsCtx = sws_getContext(width,
	                        height,
	                        // avcodec_default_get_format(cctx, &format),
	                        AV_PIX_FMT_RGBA,
	                        width,
	                        height,
	                        pixel_format,
	                        SWS_BICUBIC,
	                        0,
	                        0,
	                        0);
};

void MovieWriter::writeFrame(int frameNumber, const uint8_t* const* pixelData)
{
	// Ok some assumptions about channels here should be ok as our backing
	// Skia surface is RGBA (I think that's the N32 means). We could try to
	// optimize by having skia render RGB only since we discard the A anwyay
	// and I don't think we're compositing anything where it would matter to
	// have the alpha buffer.
	initialise_av_frame();
	int inLinesize[1] = {4 * width};

	// Run the software "scaler" really just convert from RGBA to YUV
	// here.
	sws_scale(swsCtx,
	          pixelData,
	          inLinesize,
	          0,
	          height,
	          videoFrame->data,
	          videoFrame->linesize);

	// This was kind of a guess... works ok (time seems to elapse properly
	// when playing back and durations look right). PTS is still somewhat of
	// a mystery to me, I think it just needs to be monotonically
	// incrementing but there's some extra voodoo where it won't work if you
	// just use the frame number. I used to understand this stuff...
	videoFrame->pts = frameNumber * videoStream->time_base.den /
	                  (videoStream->time_base.num * fps);

	int err;
	if ((err = avcodec_send_frame(cctx, videoFrame)) < 0)
	{
		throw std::invalid_argument(
		    string_format("Failed to send frame %i\n", err));
	}

	// Send off the packet to the encoder...
	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = nullptr;
	pkt.size = 0;
	err = avcodec_receive_packet(cctx, &pkt);
	if (err == 0)
	{
		// pkt.flags |= AV_PKT_FLAG_KEY;
		// TODO: we should probably create a timebase, so we can convert to
		// different time bases. i think our pts right now is only good for mp4
		// av_packet_rescale_ts(&pkt, cctx->time_base, videoStream->time_base);
		// pkt.stream_index = videoStream->index;

		if (av_interleaved_write_frame(ofctx, &pkt) < 0)
		{
			printf("Potential issue detected.");
		}
		av_packet_unref(&pkt);
	}
	else
	{
		// delayed frames will cause errors, but they get picked up in finalize
		// int ERROR_BUFSIZ = 1024;
		// char* errorstring = new char[ERROR_BUFSIZ];
		// av_strerror(err, errorstring, ERROR_BUFSIZ);
		// printf(errorstring);
	}

	av_frame_free(&videoFrame);
	fflush(stdout);
	printf(".");
}

void MovieWriter::finalize()
{
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
			// TODO: we should probably create a timebase, so we can convert to
			// different time bases. i think our pts right now is only good for
			// mp4 av_packet_rescale_ts(&pkt, cctx->time_base,
			// videoStream->time_base); pkt.stream_index = videoStream->index;
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
			throw std::invalid_argument(
			    string_format("Failed to close file %i\n", err));
		}
	}
}