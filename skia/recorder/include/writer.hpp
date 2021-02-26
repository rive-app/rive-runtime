#ifndef WRITER_HPP
#define WRITER_HPP

#include "util.hxx"
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

class MovieWriter
{
public:
	MovieWriter(const char* _destination, int _width, int _height, int _fps);
	void writeFrame(int frameNumber, const uint8_t* const* pixelData);
	void writeHeader();
	void finalize();

private:
	AVFrame* videoFrame;
	AVCodecContext* cctx;
	AVStream* videoStream;
	AVOutputFormat* oformat;
	AVFormatContext* ofctx;
	AVCodec* codec;
	SwsContext* swsCtx;
	AVPixelFormat pixel_format;
	const char* destinationFilename;
	void initialize();
	void initialise_av_frame();
	int width, height, fps;
	int bitrate = 400;
};

#endif