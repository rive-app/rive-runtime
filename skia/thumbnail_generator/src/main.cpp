#include "SkData.h"
#include "SkImage.h"
#include "SkStream.h"
#include "SkSurface.h"
#include "rive/file.hpp"
#include "rive/math/aabb.hpp"
#include "skia_renderer.hpp"
#include <cstdio>
#include <stdio.h>
#include <string>

std::string getFileName(char* path) {
    std::string str(path);

    const size_t from = str.find_last_of("\\/");
    const size_t to = str.find_last_of(".");
    return str.substr(from + 1, to - from - 1);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "must pass source file");
        return 1;
    }
    FILE* fp = fopen(argv[1], "rb");

    const char* outPath;
    std::string filename;
    std::string fullName;
    if (argc > 2) {
        outPath = argv[2];
    } else {
        filename = getFileName(argv[1]);
        fullName = filename + ".png";
        outPath = fullName.c_str();
    }

    if (fp == nullptr) {
        fprintf(stderr, "Failed to open rive file.\n");
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    auto length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> bytes(length);
    if (fread(bytes.data(), 1, length, fp) != length) {
        fprintf(stderr, "Failed to read rive file.\n");
        fclose(fp);
        return 1;
    }
    fclose(fp);

    auto file = rive::File::import(rive::toSpan(bytes));
    if (!file) {
        fprintf(stderr, "Failed to read rive file.\n");
        return 1;
    }
    auto artboard = file->artboardDefault();
    artboard->advance(0.0f);

    int width = 256, height = 256;

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
    if (!img) {
        return 1;
    }
    sk_sp<SkData> png(img->encodeToData());
    if (!png) {
        return 1;
    }
    SkFILEWStream out(outPath);
    (void)out.write(png->data(), png->size());
    return 0;
}
