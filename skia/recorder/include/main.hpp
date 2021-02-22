#include "SkData.h"
#include "SkImage.h"
#include "SkStream.h"
#include "SkSurface.h"
#include "animation/animation.hpp"
#include "animation/linear_animation.hpp"
#include "args.hxx"
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
std::string getFileName(const char* path);

template <typename... Args>
std::string string_format(const std::string& format, Args... args);

rive::File* getRiveFile(const char* path);