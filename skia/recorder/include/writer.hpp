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
	MovieWriter(const std::string& destination,
	            int width,
	            int height,
	            int fps,
	            int bitrate = 0);
	~MovieWriter();
	void writeHeader();
	void writeFrame(int frameNumber, const uint8_t* const* pixelData);
	void finalize() const;

private:
	AVFrame* m_VideoFrame;
	AVCodecContext* m_Cctx;
	AVStream* m_VideoStream;
	AVOutputFormat* m_OFormat;
	AVFormatContext* m_OFctx;
	AVCodec* m_Codec;
	SwsContext* m_SwsCtx;
	AVPixelFormat m_PixelFormat;
	std::string m_DestinationPath;
	int m_Width, m_Height, m_Fps, m_Bitrate;

	void initialize();
	void initialise_av_frame();
};

#endif