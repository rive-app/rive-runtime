/*
 * Copyright 2022 Rive
 */

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

std::vector<uint8_t> ViewerContent::LoadFile(const char filename[])
{
    std::vector<uint8_t> bytes;

    FILE* fp = fopen(filename, "rb");
    if (!fp)
    {
        fprintf(stderr, "Can't find file: %s\n", filename);
        return bytes;
    }

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    bytes.resize(size);
    size_t bytesRead = fread(bytes.data(), 1, size, fp);
    fclose(fp);

    if (bytesRead != size)
    {
        fprintf(stderr, "Failed to read all of %s\n", filename);
        bytes.resize(0);
    }
    return bytes;
}

void ViewerContent::DumpCounters(const char label[])
{
    assert(sizeof(gCounterNames) / sizeof(gCounterNames[0]) == rive::Counter::kLastType + 1);

    if (label == nullptr)
    {
        label = "Counters";
    }
    printf("%s:", label);
    for (int i = 0; i <= rive::Counter::kLastType; ++i)
    {
        printf(" [%s]:%d", gCounterNames[i], rive::Counter::counts[i]);
    }
    printf("\n");
}

#ifndef RIVE_SKIP_IMGUI
#include "viewer/viewer_host.hpp"

rive::Factory* ViewerContent::RiveFactory() { return ViewerHost::Factory(); }
#endif

#include "rive/text/font_hb.hpp"
rive::rcp<rive::Font> ViewerContent::DecodeFont(rive::Span<const uint8_t> span)
{
    return HBFont::Decode(span);
}
