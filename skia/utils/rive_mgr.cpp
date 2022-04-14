#include "rive_mgr.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"

struct AutoClose {
    FILE* m_File;

    AutoClose(FILE* file) : m_File(file) {}
    ~AutoClose() { this->close(); }

    void close() {
        if (m_File) {
            fclose(m_File);
            m_File = nullptr;
        }
    }
};

bool RiveMgr::load(const char filename[], const char artboard[], const char animation[]) {
    m_File = nullptr;
    m_Artboard = nullptr;
    m_Animation = nullptr;

    m_File = RiveMgr::loadFile(filename);
    if (m_File) {
        m_Artboard = m_File->artboardNamed(artboard);
        if (m_Artboard) {
            m_Animation = m_Artboard->animationNamed(animation);
        }
    }
    return m_File != nullptr;
}

static void log(const char msg[], const char arg[] = nullptr) {
    std::string err(msg);
    if (arg) {
        err = err + std::string(arg);
    }
    fprintf(stderr, "%s\n", err.c_str());
}

std::unique_ptr<rive::File> RiveMgr::loadFile(const char filename[]) {
    AutoClose afc(fopen(filename, "rb"));
    FILE* fp = afc.m_File;

    if (fp == nullptr) {
        log("Failed to open file ", filename);
        return nullptr;
    }

    fseek(fp, 0, SEEK_END);
    auto length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    std::vector<uint8_t> bytes(length);

    if (fread(bytes.data(), 1, length, fp) != length) {
        log("Failed to read file into bytes array ", filename);
        return nullptr;
    }

    auto reader = rive::BinaryReader(bytes.data(), length);
    auto file = rive::File::import(reader);

    if (!file) {
        log("Failed to read bytes into Rive file ", filename);
    }
    return file;
}
