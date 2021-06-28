#include "writer.hpp"
#include <sstream>

MovieWriter::MovieWriter(const std::string& destination,
                         int width,
                         int height,
                         int fps,
                         int bitrate) :
    m_VideoFrame(nullptr),
    m_Cctx(nullptr),
    m_VideoStream(nullptr),
    m_OFormat(nullptr),
    m_OFctx(nullptr),
    m_Codec(nullptr),
    m_SwsCtx(nullptr),
    m_PixelFormat(AV_PIX_FMT_YUV420P),
    m_DestinationPath(destination),
    m_Width(width),
    m_Height(height),
    m_Fps(fps),
    m_Bitrate(bitrate)
{
	initialize();
};

MovieWriter::~MovieWriter()
{
	sws_freeContext(m_SwsCtx);
	avformat_free_context(m_OFctx);
	avcodec_free_context(&m_Cctx);
}

void MovieWriter::initialize()
{
	auto destPath = m_DestinationPath.c_str();
	// if init fails all this stuff needs cleaning up?

	// Try to guess the output format from the name.
	m_OFormat = av_guess_format(nullptr, destPath, nullptr);
	if (!m_OFormat)
	{
		throw std::invalid_argument(
		    std::string("Failed to determine output format for ") + destPath +
		    ".");
	}

	// Get a context for the format to work with (I guess the OutputFormat
	// is sort of the blueprint, and this is the instance for this specific
	// run of it).
	m_OFctx = nullptr;
	// TODO: there's probably cleanup to do here.
	if (avformat_alloc_output_context2(&m_OFctx, m_OFormat, nullptr, destPath) <
	    0)
	{
		throw std::invalid_argument(
		    std::string("Failed to allocate output context ") + destPath + ".");
	}
	// Check that we have the necessary codec for the format we want to
	// encode (I think most formats can have multiple codecs so this
	// probably tries to guess the best default available one).
	m_Codec = avcodec_find_encoder(m_OFormat->video_codec);
	if (!m_Codec)
	{
		throw std::invalid_argument(std::string("Failed to find codec for ") +
		                            destPath + ".");
	}

	// Allocate the stream we're going to be writing to.
	m_VideoStream = avformat_new_stream(m_OFctx, m_Codec);
	if (!m_VideoStream)
	{
		throw std::invalid_argument(
		    std::string("Failed to create a stream for ") + destPath + ".");
	}

	// Similar to AVOutputFormat and AVFormatContext, the codec needs an
	// instance/"context" to store data specific to this run.
	m_Cctx = avcodec_alloc_context3(m_Codec);
	if (!m_Cctx)
	{
		throw std::invalid_argument(
		    std::string("Failed to allocate codec context for ") + destPath +
		    ".");
	}

	// default to our friend yuv, mp4 is basically locked onto this.
	m_PixelFormat = AV_PIX_FMT_YUV420P;
	if (m_OFormat->video_codec == AV_CODEC_ID_GIF)
	{
		// for some reason we dont get anything actually animating here...
		// we're getting the same frame over and over
		m_PixelFormat = AV_PIX_FMT_RGB8;

		// I think these are the formats that should work here
		// https://ffmpeg.org/doxygen/trunk/libavcodec_2gif_8c.html
		//     .pix_fmts       = (const enum AVPixelFormat[]){
		//     AV_PIX_FMT_RGB8, AV_PIX_FMT_BGR8, AV_PIX_FMT_RGB4_BYTE,
		//     AV_PIX_FMT_BGR4_BYTE, AV_PIX_FMT_GRAY8, AV_PIX_FMT_PAL8,
		//     AV_PIX_FMT_NONE
		// },
	}

	m_VideoStream->codecpar->codec_id = m_OFormat->video_codec;
	m_VideoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
	m_VideoStream->codecpar->width = m_Width;
	m_VideoStream->codecpar->height = m_Height;
	m_VideoStream->codecpar->format = m_PixelFormat;
	m_VideoStream->time_base = {1, m_Fps};

	// Yeah so these are just some numbers that work, we'll probably want to
	// fine tune these...
	avcodec_parameters_to_context(m_Cctx, m_VideoStream->codecpar);
	m_Cctx->time_base = {1, m_Fps};
	m_Cctx->framerate = {m_Fps, 1};
	m_Cctx->max_b_frames = 2;
	m_Cctx->gop_size = 12;

	if (m_VideoStream->codecpar->codec_id == AV_CODEC_ID_H264)
	{
		// Set the H264 preset to shite but fast, I guess?
		av_opt_set(m_Cctx, "preset", "ultrafast", 0);
	}
	else if (m_VideoStream->codecpar->codec_id == AV_CODEC_ID_H265)
	{
		// More beauty
		av_opt_set(m_Cctx, "preset", "ultrafast", 0);
	}

	// OK! Finally set the parameters on the stream from the codec context
	// we just fucked with.
	avcodec_parameters_from_context(m_VideoStream->codecpar, m_Cctx);

	AVDictionary* codec_options(0);
	// Add a few quality options that respect the choices above.
	av_dict_set(&codec_options, "preset", "ultrafast", 0);
	av_dict_set(&codec_options, "tune", "film", 0);
	// Set number of frames to look ahead for frametype and ratecontrol.
	av_dict_set_int(&codec_options, "rc-lookahead", 60, 0);

	// A custom Bit Rate has been specified:
	// 	enforce CBR via AVCodecContext and string parameters.
	if (m_Bitrate != 0)
	{
		int br = m_Bitrate * 1000;
		m_Cctx->bit_rate = br;
		m_Cctx->rc_min_rate = br;
		m_Cctx->rc_max_rate = br;
		m_Cctx->rc_buffer_size = br;
		m_Cctx->rc_initial_buffer_occupancy = static_cast<int>(br * 9 / 10);

		std::string strParams = "vbv-maxrate=" + std::to_string(m_Bitrate) +
		                        ":vbv-bufsize=" + std::to_string(m_Bitrate) +
		                        ":force-cfr=1:nal-hrd=cbr";
		av_dict_set(&codec_options, "x264-params", strParams.c_str(), 0);
	}

	if (avcodec_open2(m_Cctx, m_Codec, &codec_options) < 0)
	{
		throw std::invalid_argument(std::string("Failed to open codec ") +
		                            destPath);
	}
	av_dict_free(&codec_options);
}

void MovieWriter::initialise_av_frame()
{
	// Init some ffmpeg data to hold our encoded frames (convert them to the
	// right format).
	m_VideoFrame = av_frame_alloc();
	m_VideoFrame->format = m_PixelFormat;
	m_VideoFrame->width = m_Width;
	m_VideoFrame->height = m_Height;
	int err;
	if ((err = av_frame_get_buffer(m_VideoFrame, 32)) < 0)
	{
		std::ostringstream errorStream;
		errorStream << "Failed to allocate buffer for frame with error " << err;
		throw std::invalid_argument(errorStream.str());
	}
};

void MovieWriter::writeHeader()
{
	auto destPath = m_DestinationPath.c_str();
	// Finally open the file! Interesting step here, I guess some files can
	// just record to memory or something, so they don't actually need a
	// file to open io.
	if (!(m_OFormat->flags & AVFMT_NOFILE))
	{
		int err;
		if ((err = avio_open(&m_OFctx->pb, destPath, AVIO_FLAG_WRITE)) < 0)
		{
			std::ostringstream errorStream;
			errorStream << "Failed to open file " << destPath << " with error "
			            << err;
			throw std::invalid_argument(errorStream.str());
		}
	}

	// Header time...
	if (avformat_write_header(m_OFctx, NULL) < 0)
	{
		throw std::invalid_argument(
		    std::string("Failed to write header %i\n", destPath));
	}

	// Write the format into the header...
	av_dump_format(m_OFctx, 0, destPath, 1);

	// Init a software scaler to do the conversion.
	m_SwsCtx = sws_getContext(m_Width,
	                          m_Height,
	                          // avcodec_default_get_format(cctx, &format),
	                          AV_PIX_FMT_RGBA,
	                          m_Width,
	                          m_Height,
	                          m_PixelFormat,
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
	int inLinesize[1] = {4 * m_Width};

	// Run the software "scaler" really just convert from RGBA to YUV
	// here.
	sws_scale(m_SwsCtx,
	          pixelData,
	          inLinesize,
	          0,
	          m_Height,
	          m_VideoFrame->data,
	          m_VideoFrame->linesize);

	// This was kind of a guess... works ok (time seems to elapse properly
	// when playing back and durations look right). PTS is still somewhat of
	// a mystery to me, I think it just needs to be monotonically
	// incrementing but there's some extra voodoo where it won't work if you
	// just use the frame number. I used to understand this stuff...
	m_VideoFrame->pts = frameNumber * m_VideoStream->time_base.den /
	                    (m_VideoStream->time_base.num * m_Fps);

	int err;
	if ((err = avcodec_send_frame(m_Cctx, m_VideoFrame)) < 0)
	{
		std::ostringstream errorStream;
		errorStream << "Failed to send frame " << err;
		throw std::invalid_argument(errorStream.str());
	}

	// Send off the packet to the encoder...
	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = nullptr;
	pkt.size = 0;
	err = avcodec_receive_packet(m_Cctx, &pkt);
	if (err == 0)
	{
		// pkt.flags |= AV_PKT_FLAG_KEY;
		// TODO: we should probably create a timebase, so we can convert to
		// different time bases. i think our pts right now is only good for mp4
		// av_packet_rescale_ts(&pkt, cctx->time_base, videoStream->time_base);
		// pkt.stream_index = videoStream->index;

		if (av_interleaved_write_frame(m_OFctx, &pkt) < 0)
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

	av_frame_free(&m_VideoFrame);
	printf(".");
	fflush(stdout);
}

void MovieWriter::finalize() const
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
		avcodec_send_frame(m_Cctx, nullptr);
		if (avcodec_receive_packet(m_Cctx, &pkt) == 0)
		{
			// TODO: we should probably create a timebase, so we can convert to
			// different time bases. i think our pts right now is only good for
			// mp4 av_packet_rescale_ts(&pkt, cctx->time_base,
			// videoStream->time_base); pkt.stream_index = videoStream->index;
			av_interleaved_write_frame(m_OFctx, &pkt);
			av_packet_unref(&pkt);
		}
		else
		{
			break;
		}
	}
	printf(".\n");

	// Write the footer (trailer?) woo!
	av_write_trailer(m_OFctx);
	if (!(m_OFormat->flags & AVFMT_NOFILE))
	{
		int err = avio_close(m_OFctx->pb);
		if (err < 0)
		{
			std::ostringstream errorStream;
			errorStream << "Failed to close file " << err;
			throw std::invalid_argument(errorStream.str());
		}
	}
}