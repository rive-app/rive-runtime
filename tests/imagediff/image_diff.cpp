/**
 *  Copyright 2022 Rive Inc.
 */

#ifdef TESTING
#else

#include "imagediff_arguments.hpp"
#include "image_cmp.hpp"
#include "SkImage.h"
#include "SkData.h"
#include "SkPixmap.h"
#include "common/write_png_file.hpp"

#include <assert.h>
#include <string>
#include <stdio.h>

class WFile
{
    FILE* m_File = nullptr;
    bool m_DoClose;

public:
    WFile(FILE* existingFile) : m_File(existingFile) { m_DoClose = false; }

    WFile(const char path[], const char mode[]) : m_File(fopen(path, mode))
    {
        m_DoClose = true;
    }

    ~WFile() { this->close(); }

    bool isOpen() const { return m_File != nullptr; }

    bool open(const char path[], const char mode[])
    {
        this->close();
        m_File = fopen(path, mode);
        return m_File != nullptr;
    }

    void close()
    {
        if (m_DoClose && m_File)
        {
            fclose(m_File);
        }
        m_File = nullptr;
    }

    void write(const void* data, size_t length)
    {
        assert(m_File);
        fwrite(data, 1, length, m_File);
    }

    void write(const std::string& str) { this->write(str.c_str(), str.size()); }

    void writeln(const std::string& str)
    {
        this->write(str);
        this->write("\n");
    }
};

static sk_sp<SkImage> file_to_image(const char path[])
{
    auto data = SkData::MakeFromFileName(path);
    if (!data)
    {
        return nullptr;
    }
    auto img = SkImage::MakeFromEncoded(data);
    if (img)
    {
        img = img->makeRasterImage();
    }
    return img;
}

static void write_file(const char dir[],
                       const char name[],
                       const char suffix[],
                       sk_sp<SkImage> img)
{
    auto path = std::string(dir) + std::string("/") + std::string(name) +
                std::string(suffix);

    SkColorInfo colorInfo(kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr);
    std::vector<uint8_t> pixels(img->height() * img->width() * 4);
    img->readPixels(
        nullptr,
        SkPixmap(SkImageInfo::Make({img->width(), img->height()}, colorInfo),
                 pixels.data(),
                 img->width() * 4),
        0,
        0);

    //    printf("creating %s\n", path.c_str());
    WritePNGFile(pixels.data(),
                 img->width(),
                 img->height(),
                 false,
                 path.c_str(),
                 PNGCompression::fast_rle);
}

void image_diff(const char name[],
                const char pathA[],
                const char pathB[],
                const char statusPath[],
                const char diffDir[])
{
    WFile status =
        statusPath && statusPath[0] ? WFile(statusPath, "a") : WFile(stdout);
    if (!status.isOpen())
    {
        throw "failed to open status file";
    }

    status.write(std::string(name) + "\t");

    auto imgA = file_to_image(pathA);
    auto imgB = file_to_image(pathB);
    if (!imgA || !imgB)
    {
        status.writeln("failed");
        return;
    }

    const auto cmp = image_cmp(imgA, imgB);
    if (!cmp.images_loaded)
    {
        status.writeln("failed");
        return;
    }

    if (cmp.identical())
    {
        status.writeln("identical");
        return;
    }

    if (!cmp.dimensions_equal)
    {
        status.writeln("sizemismatch");
        return;
    }

    status.writeln(std::to_string(cmp.max_component_diff) + '\t' +
                   std::to_string(cmp.ave_component_diff) + '\t' +
                   std::to_string(cmp.pixel_diffcount) + '\t' +
                   std::to_string(imgA->height() * imgA->width()));

    if (diffDir && diffDir[0])
    {
        auto diff0 = make_diff_image(imgA, imgB, compute_diff0);
        auto diff1 = make_diff_image(imgA, imgB, compute_diff1);

        write_file(diffDir, name, ".diff0.png", diff0);
        write_file(diffDir, name, ".diff1.png", diff1);
    }
}

int main(int argc, const char* argv[])
{
    try
    {
        ImageDiffArguments args(argc, argv);

        image_diff(args.name().c_str(),
                   args.golden().c_str(),
                   args.candidate().c_str(),
                   args.status().c_str(),
                   args.output().c_str());
    }
    catch (const args::Completion& e)
    {
        return 0;
    }
    catch (const args::Help&)
    {
        return 0;
    }
    catch (const args::ParseError& e)
    {
        return 1;
    }
    catch (args::ValidationError e)
    {
        return 1;
    }

    return 0;
}

#endif
