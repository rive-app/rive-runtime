#include "extractor/extractor.hpp"
#include "extractor/video_extractor.hpp"
#include <sstream>

int RiveFrameExtractor::totalFrames() const
{
	int min_frames = m_MinDuration * m_Fps;
	int max_frames = m_MaxDuration * m_Fps;

	int animationFrames = m_Duration;
	int totalFrames = animationFrames;

	switch (m_Animation->loop())
	{
		case rive::Loop::pingPong:
			animationFrames *= 2;
		// No break: falls through.
		case rive::Loop::loop:
			// pingpong is like loop, just you gotta go back and forth, so 2x
			// duration
			if (min_frames != 0 && totalFrames < min_frames)
			{
				totalFrames = std::ceil(min_frames / float(animationFrames)) *
				              animationFrames;
			}
			if (max_frames != 0 && totalFrames > max_frames &&
			    animationFrames < max_frames)
			{
				totalFrames = std::floor(max_frames / (animationFrames)) *
				              animationFrames;
			}
			break;
		default:
			break;
	}
	// no matter what shenanigans we had before, lets make sure we fall
	// in line regardless.
	if (min_frames != 0 && totalFrames < min_frames)
	{
		totalFrames = min_frames;
	}
	if (max_frames != 0 && totalFrames > max_frames)
	{
		totalFrames = max_frames;
	}
	return totalFrames;
};

void RiveFrameExtractor::initializeDimensions(int width,
                                              int height,
                                              int small_extent_target,
                                              int max_width,
                                              int max_height)
{
	// Take the width & height from user input, or from the provided artboard
	m_Width = valueOrDefault(width, m_Artboard->width());
	m_Height = valueOrDefault(height, m_Artboard->height());

	// if we have a target value for whichever extent is smaller, lets scale to
	// that.
	if (small_extent_target != 0)
	{
		if (m_Width < m_Height)
		{
			scale(&m_Width, small_extent_target, &m_Height);
		}
		else
		{
			scale(&m_Height, small_extent_target, &m_Width);
		}
	}

	// if we have a max height, lets scale down to that
	if (max_height != 0 && max_height < m_Height)
	{
		scale(&m_Height, max_height, &m_Width);
	}

	// if we have a max width, lets scale down to that
	if (max_width != 0 && max_width < m_Width)
	{
		scale(&m_Width, max_width, &m_Height);
	}

	// We're sticking with 2's right now. so you know it is what it is.
	m_Width = nextMultipleOf(m_Width, 2);
	m_Height = nextMultipleOf(m_Height, 2);
}

sk_sp<SkImage>
RiveFrameExtractor::getWatermark(const char* watermark_name) const
{
	// Init skia surfaces to render to.
	sk_sp<SkImage> watermarkImage;
	if (watermark_name != NULL && watermark_name[0] != '\0')
	{

		if (!file_exists(watermark_name))
		{
			std::ostringstream errorStream;
			errorStream << "Cannot find file containing watermark at "
			            << watermark_name;
			throw std::invalid_argument(errorStream.str());
		}
		if (auto data = SkData::MakeFromFileName(watermark_name))
		{
			watermarkImage = SkImage::MakeFromEncoded(data);
		}
	}
	return watermarkImage;
}

rive::File* RiveFrameExtractor::getRiveFile(const char* path) const
{
	FILE* fp = fopen(path, "r");

	if (fp == nullptr)
	{
		fclose(fp);
		std::ostringstream errorStream;
		errorStream << "Failed to open file " << path;
		throw std::invalid_argument(errorStream.str());
	}
	fseek(fp, 0, SEEK_END);
	auto length = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	uint8_t* bytes = new uint8_t[length];

	if (fread(bytes, 1, length, fp) != length)
	{
		fclose(fp);
		delete[] bytes;
		std::ostringstream errorStream;
		errorStream << "Failed to read file into bytes array " << path;
		throw std::invalid_argument(errorStream.str());
	}

	auto reader = rive::BinaryReader(bytes, length);
	rive::File* file = nullptr;
	auto result = rive::File::import(reader, &file);

	fclose(fp);
	delete[] bytes;

	if (result != rive::ImportResult::success)
	{
		std::ostringstream errorStream;
		errorStream << "Failed to read bytes into Rive file " << path;
		throw std::invalid_argument(errorStream.str());
	}
	return file;
}

rive::Artboard* RiveFrameExtractor::getArtboard(const char* artboard_name) const
{
	// Figure out which artboard to use.
	rive::Artboard* artboard;

	// QUESTION: better to keep this logic in the main? or pass the flag
	// in here, so we can bool check if the flag is set or not? what
	// happens if we try to target the artboard '' otherwise?
	//
	if (artboard_name != NULL && artboard_name[0] != '\0')
	{
		if ((artboard = m_RiveFile->artboard(artboard_name)) == nullptr)
		{
			std::ostringstream errorStream;
			errorStream << "File doesn't contain an artboard named "
			            << artboard_name;
			throw std::invalid_argument(errorStream.str());
		}
	}
	else
	{
		artboard = m_RiveFile->artboard();
		if (artboard == nullptr)
		{
			throw std::invalid_argument(
			    "File doesn't contain a default artboard.");
		}
	}
	return artboard;
}

rive::LinearAnimation*
RiveFrameExtractor::getAnimation(const char* animation_name) const
{
	// Figure out which animation to use.
	rive::LinearAnimation* animation;
	if (animation_name != NULL && animation_name[0] != '\0')
	{
		if ((animation = m_Artboard->animation(animation_name)) == nullptr)
		{
			std::ostringstream errorStream;
			errorStream << "Artboard doesn't contain an animation named "
			            << animation_name;
			throw std::invalid_argument(errorStream.str());
		}
	}
	else
	{
		animation = m_Artboard->firstAnimation();
		if (animation == nullptr)
		{
			throw std::invalid_argument(
			    "Artboard doesn't contain a default animation.");
		}
	}
	return animation;
};

void RiveFrameExtractor::advanceFrame() const
{
	m_AnimationInstance->advance(m_IFps);
}

void RiveFrameExtractor::restart() const
{
	m_AnimationInstance->time(m_Animation->startSeconds());
}

sk_sp<SkImage>
RiveFrameExtractor::getSnapshot(SkColor clearColor = SK_ColorBLACK) const
{

	// I see a canvas and I want to paint it black.
	// (without this transparent background dont get cleared.)
	SkPaint paint;
	m_RasterCanvas->clear(clearColor);

	// hmm "no deafault constructor exists bla bla... "
	rive::SkiaRenderer renderer(m_RasterCanvas);

	renderer.save();
	renderer.align(rive::Fit::cover,
	               rive::Alignment::center,
	               rive::AABB(0, 0, width(), height()),
	               m_Artboard->bounds());
	m_AnimationInstance->apply(m_Artboard);
	m_Artboard->advance(0.0f);
	m_Artboard->draw(&renderer);
	renderer.restore();
	if (m_WatermarkImage)
	{
		SkPaint watermarkPaint;
		watermarkPaint.setBlendMode(SkBlendMode::kDifference);
		m_RasterCanvas->drawImage(m_WatermarkImage,
		                          width() - m_WatermarkImage->width() - 50,
		                          height() - m_WatermarkImage->height() - 50,
		                          &watermarkPaint);
	}

	// After drawing the frame, grab the raw image data.
	sk_sp<SkImage> img(m_RasterSurface->makeImageSnapshot());
	if (!img)
	{
		throw std::invalid_argument("Cant make a snapshot.");
	}
	return img;
}

const void* RiveFrameExtractor::getPixelAddresses() const
{
	auto img = getSnapshot();
	SkPixmap pixels;
	if (!img->peekPixels(&pixels))
	{
		throw std::invalid_argument(
		    "Cant peek pixels image frame from riv file.");
	}

	// Get the address to the first pixel (addr8 will assert in debug mode
	// as Skia only wants you to use that with 8 bit surfaces).
	return pixels.addr(0, 0);
};

sk_sp<SkData>
RiveFrameExtractor::getSkData(SkColor clearColor = SK_ColorBLACK) const
{
	auto img = getSnapshot(clearColor);
	sk_sp<SkData> png(img->encodeToData());
	if (!png)
	{
		throw std::invalid_argument("Cant encode snapshot as png.");
	}

	// Get the address to the first pixel (addr8 will assert in debug mode
	// as Skia only wants you to use that with 8 bit surfaces).
	return png;
};

void RiveFrameExtractor::extractFrames(int numLoops)
{
	int totalFrames = this->totalFrames();
	for (int loops = 0; loops < numLoops; loops++)
	{
		// Reset the animation time to the start
		this->restart();
		for (int i = 0; i < totalFrames; i++)
		{
			this->advanceFrame();
			int frameNumber = loops * totalFrames + i;
			this->onNextFrame(frameNumber);
		}
	}
}

void RiveFrameExtractor::takeSnapshot(const std::string& snapshotPath) const
{
	if (snapshotPath.empty())
	{
		return;
	}

	this->restart();

	this->advanceFrame();
	SkFILEWStream out(snapshotPath.c_str());
	auto png = this->getSkData();
	(void)out.write(png->data(), png->size());

	// Rewind back to the start.
	this->restart();
}