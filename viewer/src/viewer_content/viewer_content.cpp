#ifdef RIVE_RENDERER_TESS
#include "viewer/tess/viewer_sokol_factory.hpp"
#endif
#ifdef RIVE_RENDERER_SKIA
#include "skia_factory.hpp"
#endif

#include "viewer/viewer_content.hpp"
#include "rive/rive_counter.hpp"
#include <vector>

ViewerContent::~ViewerContent() { DumpCounters("After deleting content"); }

const char* gCounterNames[] = {
    "file",
    "artboard",
    "animation",
    "machine",
    "buffer",
    "path",
    "paint",
    "shader",
    "image",
};

std::vector<uint8_t> ViewerContent::LoadFile(const char filename[]) {
    std::vector<uint8_t> bytes;

    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Can't find file: %s\n", filename);
        return bytes;
    }

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    bytes.resize(size);
    size_t bytesRead = fread(bytes.data(), 1, size, fp);
    fclose(fp);

    if (bytesRead != size) {
        fprintf(stderr, "Failed to read all of %s\n", filename);
        bytes.resize(0);
    }
    return bytes;
}

void ViewerContent::DumpCounters(const char label[]) {
    assert(sizeof(gCounterNames) / sizeof(gCounterNames[0]) == rive::Counter::kLastType + 1);

    if (label == nullptr) {
        label = "Counters";
    }
    printf("%s:", label);
    for (int i = 0; i <= rive::Counter::kLastType; ++i) {
        printf(" [%s]:%d", gCounterNames[i], rive::Counter::counts[i]);
    }
    printf("\n");
}

rive::Factory* ViewerContent::RiveFactory() {
#ifdef RIVE_RENDERER_TESS
    static ViewerSokolFactory sokolFactory;
    return &sokolFactory;
#endif
#ifdef RIVE_RENDERER_SKIA
    static rive::SkiaFactory skiaFactory;
    return &skiaFactory;
#endif
}