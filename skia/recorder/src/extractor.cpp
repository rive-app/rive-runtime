#include "extractor.hpp"

int valueOrDefault(int value, int default_value)
{
	if (value == 0)
	{
		return default_value;
	}
	return value;
}

void scale(int* value, int targetValue, int* otherValue)
{
	if (*value != targetValue)
	{
		*otherValue = *otherValue * targetValue / *value;
		*value = targetValue;
	}
}

RiveFrameExtractor::RiveFrameExtractor(const char* path,
                                       const char* artboard_name,
                                       const char* animation_name,
                                       const char* watermark_name,
                                       int width,
                                       int height,
                                       int small_extent_target,
                                       int max_width,
                                       int max_height,
                                       int min_duration,
                                       int max_duration,
                                       float fps)
{
	_min_duration = min_duration;
	_max_duration = max_duration;
	riveFile = getRiveFile(path);
	artboard = getArtboard(artboard_name);
	animation = getAnimation(animation_name);
	animation_instance = new rive::LinearAnimationInstance(animation);
	watermarkImage = getWaterMark(watermark_name);
	initializeDimensions(
	    width, height, small_extent_target, max_width, max_height);
	rasterSurface = SkSurface::MakeRaster(SkImageInfo::Make(
	    _width, _height, kRGBA_8888_SkColorType, kPremul_SkAlphaType));
	rasterCanvas = rasterSurface->getCanvas();
	_fps = fps <= 0 ? animation->fps() : fps;
	ifps = 1.0 / fps;
};

RiveFrameExtractor::~RiveFrameExtractor()
{
	if (animation_instance)
	{
		delete animation_instance;
	}
	if (riveFile)
	{
		delete riveFile;
	}
}

int RiveFrameExtractor::width() { return _width; };
int RiveFrameExtractor::height() { return _height; };
float RiveFrameExtractor::fps() { return _fps; };
int RiveFrameExtractor::totalFrames()
{
	int min_frames = _min_duration * fps();
	int max_frames = _max_duration * fps();

	int animationFrames = animation->duration();
	int totalFrames = animation->duration();
	// TODO: combine those two into one function
	switch (animation->loop())
	{
		case rive::Loop::pingPong:
			animationFrames *= 2;
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
	// no matter what shenanigans we had before, lets make sure we fall in line
	// regardless.
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
	_width = valueOrDefault(width, artboard->width());
	_height = valueOrDefault(height, artboard->height());

	// if we have a target value for whichever extent is smaller, lets scale to
	// that.
	if (small_extent_target != 0)
	{
		if (_width < _height)
		{
			scale(&_width, small_extent_target, &_height);
		}
		else
		{
			scale(&_height, small_extent_target, &_width);
		}
	}

	// if we have a max height, lets scale down to that
	if (max_height != 0 && max_height < _height)
	{
		scale(&_height, max_height, &_width);
	}

	// if we have a max width, lets scale down to that
	if (max_width != 0 && max_width < _width)
	{
		scale(&_width, max_width, &_height);
	}

	// We're sticking with 2's right now. so you know it is what it is.
	_width = nextMultipleOf(_width, 2);
	_height = nextMultipleOf(_height, 2);
}

sk_sp<SkImage> RiveFrameExtractor::getWaterMark(const char* watermark_name)
{
	// Init skia surfaces to render to.
	sk_sp<SkImage> watermarkImage;
	if (watermark_name != NULL && watermark_name[0] != '\0')
	{

		if (!file_exists(watermark_name))
		{
			throw std::invalid_argument(string_format(
			    "Cannot find file containing watermark at %s", watermark_name));
		}
		if (auto data = SkData::MakeFromFileName(watermark_name))
		{
			watermarkImage = SkImage::MakeFromEncoded(data);
		}
	}
	return watermarkImage;
}

rive::File* RiveFrameExtractor::getRiveFile(const char* path)
{
	FILE* fp = fopen(path, "r");

	if (fp == nullptr)
	{
		fclose(fp);
		throw std::invalid_argument(
		    string_format("Failed to open file %s", path));
	}
	fseek(fp, 0, SEEK_END);
	auto length = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	uint8_t* bytes = new uint8_t[length];

	if (fread(bytes, 1, length, fp) != length)
	{
		fclose(fp);
		delete[] bytes;
		throw std::invalid_argument(
		    string_format("Failed to read file into bytes array %s", path));
	}

	auto reader = rive::BinaryReader(bytes, length);
	rive::File* file = nullptr;
	auto result = rive::File::import(reader, &file);

	fclose(fp);
	delete[] bytes;

	if (result != rive::ImportResult::success)
	{
		throw std::invalid_argument(
		    string_format("Failed to read bytes into Rive file %s", path));
	}
	return file;
}

rive::Artboard* RiveFrameExtractor::getArtboard(const char* artboard_name)
{
	// Figure out which artboard to use.
	rive::Artboard* artboard;

	// QUESTION: better to keep this logic in the main? or pass the flag
	// in here, so we can bool check if the flag is set or not? what
	// happens if we try to target the artboard '' otherwise?
	//
	if (artboard_name != NULL && artboard_name[0] != '\0')
	{
		if ((artboard = riveFile->artboard(artboard_name)) == nullptr)
		{
			throw std::invalid_argument(string_format(
			    "File doesn't contain an artboard named %s.", artboard_name));
		}
	}
	else
	{
		artboard = riveFile->artboard();
		if (artboard == nullptr)
		{
			throw std::invalid_argument(string_format(
			    "File doesn't contain a default artboard.", artboard_name));
		}
	}
	return artboard;
}

rive::LinearAnimation*
RiveFrameExtractor::getAnimation(const char* animation_name)
{
	// Figure out which animation to use.
	rive::LinearAnimation* animation;
	if (animation_name != NULL && animation_name[0] != '\0')
	{
		if ((animation = artboard->animation(animation_name)) == nullptr)
		{

			fprintf(stderr,
			        "Artboard doesn't contain an animation named %s.\n",
			        animation_name);
		}
	}
	else
	{
		animation = artboard->firstAnimation();
		if (animation == nullptr)
		{
			throw std::invalid_argument(
			    string_format("Artboard doesn't contain a default animation."));
		}
	}
	return animation;
};

void RiveFrameExtractor::advanceFrame() { animation_instance->advance(ifps); }

sk_sp<SkImage> RiveFrameExtractor::getSnapshot()
{

	// I see a canvas and I want to paint it black.
	// (without this transparent background dont get cleared.)
	SkPaint paint;
	rasterCanvas->clear(SK_ColorBLACK);

	// hmm "no deafault constructor exists bla bla... "
	rive::SkiaRenderer renderer(rasterCanvas);

	renderer.save();
	renderer.align(rive::Fit::cover,
	               rive::Alignment::center,
	               rive::AABB(0, 0, width(), height()),
	               artboard->bounds());
	animation_instance->apply(artboard);
	artboard->advance(0.0f);
	artboard->draw(&renderer);
	renderer.restore();
	if (watermarkImage)
	{
		SkPaint watermarkPaint;
		watermarkPaint.setBlendMode(SkBlendMode::kDifference);
		rasterCanvas->drawImage(watermarkImage,
		                        width() - watermarkImage->width() - 50,
		                        height() - watermarkImage->height() - 50,
		                        &watermarkPaint);
	}

	// After drawing the frame, grab the raw image data.
	sk_sp<SkImage> img(rasterSurface->makeImageSnapshot());
	if (!img)
	{
		throw std::invalid_argument(string_format("Cant make a snapshot."));
	}
	return img;
}

const void* RiveFrameExtractor::getPixelAddresses()
{
	auto img = getSnapshot();
	SkPixmap pixels;
	if (!img->peekPixels(&pixels))
	{
		throw std::invalid_argument(
		    string_format("Cant peek pixels image frame from riv file."));
	}

	// Get the address to the first pixel (addr8 will assert in debug mode
	// as Skia only wants you to use that with 8 bit surfaces).
	return pixels.addr(0, 0);
};

sk_sp<SkData> RiveFrameExtractor::getSkData()
{
	auto img = getSnapshot();
	sk_sp<SkData> png(img->encodeToData());
	if (!png)
	{
		throw std::invalid_argument(
		    string_format("Cant encode snapshot as png."));
	}

	// Get the address to the first pixel (addr8 will assert in debug mode
	// as Skia only wants you to use that with 8 bit surfaces).
	return png;
};