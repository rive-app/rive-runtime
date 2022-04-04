#ifndef _RIVE_FILE_READER_HPP_
#define _RIVE_FILE_READER_HPP_

#include <rive/core/binary_reader.hpp>
#include <rive/file.hpp>
#include "rive_testing.hpp"

class RiveFileReader {
    std::unique_ptr<rive::File> m_File;
    uint8_t* m_Bytes = nullptr;
    rive::BinaryReader* m_Reader;

public:
    RiveFileReader(const char path[]) {
        FILE* fp = fopen(path, "r");
        REQUIRE(fp != nullptr);

        fseek(fp, 0, SEEK_END);
        const size_t length = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        m_Bytes = new uint8_t[length];
        REQUIRE(fread(m_Bytes, 1, length, fp) == length);
        m_Reader = new rive::BinaryReader(m_Bytes, length);
        rive::ImportResult result;
        m_File = rive::File::import(*m_Reader, &result);

        REQUIRE(result == rive::ImportResult::success);
        REQUIRE(m_File != nullptr);
        REQUIRE(m_File->artboard() != nullptr);
    }
    ~RiveFileReader() {
        delete m_Reader;
        delete[] m_Bytes;
    }

    rive::File* file() const { return m_File.get(); }
};

#endif
