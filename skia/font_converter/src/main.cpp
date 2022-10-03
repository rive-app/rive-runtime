#ifdef TESTING
#else

#include "font.h"
#include "font_arguments.hpp"
#include "include/core/SkData.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkStream.h"
#include "include/core/SkTypeface.h"
#include <vector>

static std::vector<char> readFile(const char path[])
{
    FILE* fp = fopen(path, "rb");

    if (fp == nullptr)
    {
        fclose(fp);
        std::ostringstream errorStream;
        errorStream << "Failed to open file " << path;
        throw std::invalid_argument(errorStream.str());
    }
    fseek(fp, 0, SEEK_END);
    size_t length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    std::vector<char> data;
    data.resize(length);

    if (fread(data.data(), 1, length, fp) != length)
    {
        fclose(fp);
        std::ostringstream errorStream;
        errorStream << "Failed to read file into bytes array " << path;
        throw std::invalid_argument(errorStream.str());
    }

    fclose(fp);
    return data;
}

static void writeFile(const char path[], const void* bytes, size_t length)
{
    FILE* pFile = fopen(path, "wb");
    fwrite(bytes, sizeof(char), length, pFile);
    fclose(pFile);
}

static std::vector<char> build_default_charset()
{
    std::vector<char> charset;
    for (int i = 32; i < 127; ++i)
    {
        charset.push_back(i);
    }
    return charset;
}

int main(int argc, const char* argv[])
{
    try
    {
        FontArguments args(argc, argv);
        sk_sp<SkTypeface> typeface;

        auto src = readFile(args.source().c_str());
        auto srcData = SkData::MakeWithCopy(src.data(), src.size());
        typeface = SkTypeface::MakeFromData(srcData);
        if (!typeface)
        {
            fprintf(stderr, "Failed to convert file to SkTypeface\n");
            return 1;
        }

        std::vector<char> charset;
        auto charsetFile = args.charset();
        if (charsetFile.size() > 0)
        {
            charset = readFile(args.charset().c_str());
        }
        else
        {
            charset = build_default_charset();
        }

        RiveFont font;
        font.load(typeface, charset.data(), charset.size());

        auto dst = font.encode();

        writeFile(args.destination().c_str(), dst->data(), dst->size());
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
