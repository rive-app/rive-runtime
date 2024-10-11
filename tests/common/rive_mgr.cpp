#include "rive_mgr.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"
#include "rive/file.hpp"

struct AutoClose
{
    FILE* m_File;

    AutoClose(FILE* file) : m_File(file) {}
    ~AutoClose() { this->close(); }

    void close()
    {
        if (m_File)
        {
            fclose(m_File);
            m_File = nullptr;
        }
    }
};

RiveMgr::RiveMgr(rive::Factory* factory) : m_Factory(factory) {}
RiveMgr::~RiveMgr() {}

bool RiveMgr::loadArtboard(const char filename[], const char artboard[])
{
    m_File = nullptr;
    m_Artboard = nullptr;
    m_Scene = nullptr;

    m_File = RiveMgr::loadFile(filename, m_Factory);
    if (m_File)
    {
        m_Artboard = (artboard && artboard[0]) ? m_File->artboardNamed(artboard)
                                               : m_File->artboardDefault();
    }
    return m_Artboard != nullptr;
}

bool RiveMgr::loadAnimation(const char filename[],
                            const char artboard[],
                            const char animation[])
{
    if (this->loadArtboard(filename, artboard))
    {
        m_Scene = (animation && animation[0])
                      ? m_Artboard->animationNamed(animation)
                      : m_Artboard->animationAt(0);
    }
    return m_Scene != nullptr;
}

bool RiveMgr::loadMachine(const char filename[],
                          const char artboard[],
                          const char machine[])
{
    if (this->loadArtboard(filename, artboard))
    {
        m_Scene = (machine && machine[0])
                      ? m_Artboard->stateMachineNamed(machine)
                      : m_Artboard->stateMachineAt(0);
    }
    return m_Scene != nullptr;
}

static void log(const char msg[], const char arg[] = nullptr)
{
    std::string err(msg);
    if (arg)
    {
        err = err + std::string(arg);
    }
    fprintf(stderr, "%s\n", err.c_str());
}

std::unique_ptr<rive::File> RiveMgr::loadFile(const char filename[],
                                              rive::Factory* factory)
{
    AutoClose afc(fopen(filename, "rb"));
    FILE* fp = afc.m_File;

    if (fp == nullptr)
    {
        log("Failed to open file ", filename);
        return nullptr;
    }

    fseek(fp, 0, SEEK_END);
    auto length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    std::vector<uint8_t> bytes(length);

    if (fread(bytes.data(), 1, length, fp) != length)
    {
        log("Failed to read file into bytes array ", filename);
        return nullptr;
    }

    auto file = rive::File::import(bytes, factory);

    if (!file)
    {
        log("Failed to read bytes into Rive file ", filename);
    }
    return file;
}
