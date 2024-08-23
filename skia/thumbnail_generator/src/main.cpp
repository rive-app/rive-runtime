#include "SkData.h"
#include "SkImage.h"
#include "SkStream.h"
#include "SkSurface.h"
#include "rive/file.hpp"
#include "rive/math/aabb.hpp"
#include "skia_factory.hpp"
#include "skia_renderer.hpp"
#include <cstdio>
#include <stdio.h>
#include <string>
#include <iostream>
#include <sstream>

std::string getFileName(char* path)
{
    std::string str(path);

    const size_t from = str.find_last_of("\\/");
    const size_t to = str.find_last_of(".");
    return str.substr(from + 1, to - from - 1);
}

const int DEFAULT_SIZE = 256;

int getArg(char* arg)
{
    std::istringstream ss(arg);
    int x;
    if (!(ss >> x))
    {
        std::cerr << "Invalid number: " << arg << '\n';
    }
    else if (!ss.eof())
    {
        std::cerr << "Trailing chars after number: " << arg << '\n';
    }
    return x == 0 ? DEFAULT_SIZE : x;
}

int main(int argc, char* argv[])
{
    rive::SkiaFactory factory;

    if (argc < 2)
    {
        fprintf(stderr, "must pass source file");
        return 1;
    }
    FILE* fp = fopen(argv[1], "rb");

    const char* outPath;
    std::string filename;
    std::string fullName;
    if (argc > 2)
    {
        outPath = argv[2];
    }
    else
    {
        filename = getFileName(argv[1]);
        fullName = filename + ".png";
        outPath = fullName.c_str();
    }

    int width = DEFAULT_SIZE, height = DEFAULT_SIZE;
    if (argc == 5)
    {
        width = getArg(argv[3]);
        height = getArg(argv[4]);
    }

    if (fp == nullptr)
    {
        fprintf(stderr, "Failed to open rive file.\n");
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    auto length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> bytes(length);
    if (fread(bytes.data(), 1, length, fp) != length)
    {
        fprintf(stderr, "Failed to read rive file.\n");
        fclose(fp);
        return 1;
    }
    fclose(fp);

    auto file = rive::File::import(bytes, &factory);
    if (!file)
    {
        fprintf(stderr, "Failed to read rive file.\n");
        return 1;
    }
    auto artboard = file->artboardDefault();
    artboard->advance(0.0f);

    sk_sp<SkSurface> rasterSurface = SkSurface::MakeRasterN32Premul(width, height);
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
    SkFILEWStream out(outPath);
    (void)out.write(png->data(), png->size());
    return 0;
}
