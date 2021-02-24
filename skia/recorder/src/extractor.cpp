#include "extractor.hpp"
#include "SkImage.h"
#include "animation/linear_animation.hpp"
#include "artboard.hpp"
#include "file.hpp"

RiveFrameExtractor::RiveFrameExtractor(const char* path,
                                       const char* artboard_name,
                                       const char* animation_name,
                                       const char* watermark_name)
{
	riveFile = getRiveFile(path);
	artboard = getArtboard(artboard_name);
	animation = getAnimation(animation_name);
	watermarkImage = getWaterMark(watermark_name);
};

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
			    "File doesn't contain an default artboard.", artboard_name));
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
			throw std::invalid_argument(
			    string_format("Artboard doesn't contain a default animation."));
		}
	}
	return animation;
};