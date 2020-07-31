#include "SkData.h"
#include "SkImage.h"
#include "SkStream.h"
#include "SkSurface.h"
#include "core/binary_reader.hpp"
#include "file.hpp"
#include "math/aabb.hpp"
#include "skia_renderer.hpp"
#include <cstdio>
#include <stdio.h>

int main()
{
	FILE* fp = fopen("../../assets/juice.riv", "r");
	if (fp == nullptr)
	{
		fprintf(stderr, "Failed to open rive file.\n");
		return 1;
	}
	fseek(fp, 0, SEEK_END);
	auto length = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	uint8_t* bytes = new uint8_t[length];
	if (fread(bytes, 1, length, fp) != length)
	{
		fprintf(stderr, "Failed to read rive file.\n");
		return 1;
	}

	auto reader = rive::BinaryReader(bytes, length);
	rive::File* file = nullptr;
	auto result = rive::File::import(reader, &file);
	if (result != rive::ImportResult::success)
	{
		fprintf(stderr, "Failed to read rive file.\n");
		return 1;
	}
	auto artboard = file->artboard();
	artboard->advance(0.0f);

	delete[] bytes;

	int width = 256, height = 256;
	const char* path = "test.png";
	sk_sp<SkSurface> rasterSurface =
	    SkSurface::MakeRasterN32Premul(width, height);
	SkCanvas* rasterCanvas = rasterSurface->getCanvas();

	rive::SkiaRenderer renderer(rasterCanvas);
	renderer.save();
	renderer.align(rive::Fit::cover,
	               rive::Alignment::center,
	               rive::AABB(0, 0, width, height),
	               artboard->bounds());
	artboard->draw(&renderer);
	renderer.restore();

	sk_sp<SkImage> img(rasterSurface->makeImageSnapshot());
	if (!img)
	{
		return 1;
	}
	sk_sp<SkData> png(img->encodeToData());
	if (!png)
	{
		return 1;
	}
	SkFILEWStream out(path);
	(void)out.write(png->data(), png->size());
	return 0;
}