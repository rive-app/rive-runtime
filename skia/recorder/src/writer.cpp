#include "writer.hpp"
#include "SkData.h"
#include "SkImage.h"
#include "SkStream.h"
#include "SkSurface.h"
#include "animation/animation.hpp"
#include "animation/linear_animation.hpp"
#include "artboard.hpp"
#include "core/binary_reader.hpp"
#include "file.hpp"
#include "math/aabb.hpp"
#include "skia_renderer.hpp"

#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <stdio.h>
#include <string>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavcodec/avfft.h>

#include <libavdevice/avdevice.h>

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

MovieWriter::MovieWriter(const char* _destination,
                         int _width,
                         int _height,
                         int _fps)
{
	destinationFilename = _destination;
	width = _width;
	height = _height;
	fps = _fps;
	initialize();
};

AVFrame* MovieWriter::get_av_frame()
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

void MovieWriter::initialize()
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
		throw std::invalid_argument(string_format(
		    "Failed to allocate output context %s\n.", destinationFilename));
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
		throw std::invalid_argument(
		    string_format("Failed to open codec %i\n", destinationFilename));
	}

	// Finally open the file! Interesting step here, I guess some files can
	// just record to memory or something, so they don't actually need a
	// file to open io.
	if (!(oformat->flags & AVFMT_NOFILE))
	{
		if (avio_open(&ofctx->pb, destinationFilename, AVIO_FLAG_WRITE) < 0)
		{
			throw std::invalid_argument(string_format(
			    "Failed to open file %s with error %i\n", destinationFilename));
		}
	}

	// Header time...
	if (avformat_write_header(ofctx, NULL) < 0)
	{
		throw std::invalid_argument(
		    string_format("Failed to write header %i\n", destinationFilename));
	}

	// Write the format into the header...
	av_dump_format(ofctx, 0, destinationFilename, 1);
}