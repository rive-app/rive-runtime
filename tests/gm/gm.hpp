/*
 * Copyright 2022 Rive
 */

#ifndef _RIVEGM_GM_HPP_
#define _RIVEGM_GM_HPP_

#include "common/testing_window.hpp"
#include "rive/renderer.hpp"
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace rivegm
{

class GM
{
    const int m_Width, m_Height;

public:
    GM(int width, int height) : m_Width(width), m_Height(height) {}
    virtual ~GM() {}

    int width() const { return m_Width; }
    int height() const { return m_Height; }

    void onceBeforeDraw() { this->onOnceBeforeDraw(); }

    // Calls clearColor(), updateFrameOptions(),
    // TestingWindow::beginFrame(), draw(), TestingWindow::flush(). (Most GMs
    // just need to override onDraw() instead of overriding this method.)
    virtual void run(std::vector<uint8_t>* pixels);

    virtual rive::ColorInt clearColor() const { return 0xffffffff; }

    virtual void updateFrameOptions(TestingWindow::FrameOptions*) const {}

    void draw(rive::Renderer*);

protected:
    virtual void onOnceBeforeDraw() {}
    virtual void onDraw(rive::Renderer*) = 0;
};

using FuncPtr = void* (*)();
// let this be used in extern c declares
extern "C" struct FactoryRegistry
{
    FuncPtr m_Value;
    const char* m_name;
    bool m_isSlow;
};

extern "C" FactoryRegistry make_factory_regsitry(FuncPtr value,
                                                 const char* name,
                                                 bool isSlow);
using GMFactory = std::unique_ptr<GM> (*)();
using GMRegistry = FactoryRegistry;

static std::vector<FactoryRegistry> RegistryList;

#define registry_get(registry)                                                 \
    std::unique_ptr<GM>(static_cast<GM*>(registry.m_Value()));
} // namespace rivegm

// Macro helpers (lifted from SkMacros.h
#define RIVE_STRING(A) RIVE_STRING_IMPL_PRIV(A)
#define RIVE_STRING_IMPL_PRIV(A) #A
#define RIVE_MACRO_CONCAT(X, Y) RIVE_MACRO_CONCAT_IMPL_PRIV(X, Y)
#define RIVE_MACRO_CONCAT_IMPL_PRIV(X, Y) X##Y

#define DECLARE_GMREGISTRY(name)                                               \
    extern "C" GMRegistry RIVE_MACRO_CONCAT(name, _get_registry)();

// Usage: GMREGISTER( return new mygmclass(...) )
//
#define GMREGISTER(name, code)                                                 \
    DECLARE_GMREGISTRY(name)                                                   \
    GMRegistry RIVE_MACRO_CONCAT(name, _get_registry)()                        \
    {                                                                          \
        return make_factory_regsitry(                                          \
            []() { return (void*)([]() { code; }()); },                        \
            RIVE_STRING(name),                                                 \
            false);                                                            \
    }
#define GMREGISTER_SLOW(name, code)                                            \
    DECLARE_GMREGISTRY(name)                                                   \
    GMRegistry RIVE_MACRO_CONCAT(name, _get_registry)()                        \
    {                                                                          \
        return make_factory_regsitry(                                          \
            []() { return (void*)([]() { code; }()); },                        \
            RIVE_STRING(name),                                                 \
            true);                                                             \
    }

#define CALL_GMREGISTRY(name)                                                  \
    auto RIVE_MACRO_CONCAT(registry, name) =                                   \
        RIVE_MACRO_CONCAT(name, _get_registry)();                              \
    if (RIVE_MACRO_CONCAT(registry, name).m_isSlow)                            \
        RegistryList.insert(RegistryList.begin(),                              \
                            std::move(RIVE_MACRO_CONCAT(registry, name)));     \
    else                                                                       \
        RegistryList.push_back(std::move(RIVE_MACRO_CONCAT(registry, name)));
// Usage:
//
//   DEF_SIMPLE_GM(name, width, height, renderer) {
//       renderer->...
//   }
//
#define DEF_SIMPLE_GM(NAME, WIDTH, HEIGHT, RENDERER)                           \
    class NAME##_GM : public rivegm::GM                                        \
    {                                                                          \
    public:                                                                    \
        NAME##_GM() : GM(WIDTH, HEIGHT) {}                                     \
        void onDraw(rive::Renderer*) override;                                 \
    };                                                                         \
    GMREGISTER(NAME, return new NAME##_GM)                                     \
    void NAME##_GM::onDraw(rive::Renderer* RENDERER)

// Usage:
//
//   DEF_SIMPLE_GM_WITH_CLEAR_COLOR(name, clearColor, width, height, renderer) {
//       renderer->...
//   }
//
#define DEF_SIMPLE_GM_WITH_CLEAR_COLOR(NAME,                                   \
                                       CLEAR_COLOR,                            \
                                       WIDTH,                                  \
                                       HEIGHT,                                 \
                                       RENDERER)                               \
    class NAME##_GM : public rivegm::GM                                        \
    {                                                                          \
    public:                                                                    \
        NAME##_GM() : GM(WIDTH, HEIGHT) {}                                     \
        ColorInt clearColor() const override { return CLEAR_COLOR; }           \
        void onDraw(rive::Renderer*) override;                                 \
    };                                                                         \
    GMREGISTER(NAME, return new NAME##_GM)                                     \
    void NAME##_GM::onDraw(rive::Renderer* RENDERER)

#endif

#ifdef RIVE_UNREAL
typedef int REGISTRY_HANDLE;
#define INVALID_REGISTRY -1
REGISTRY_HANDLE gms_get_registry_head();
REGISTRY_HANDLE gms_registry_get_next(REGISTRY_HANDLE position_handle);
void gms_build_registry();
bool gms_run_gm(REGISTRY_HANDLE gm_handle);
bool gms_registry_get_name(REGISTRY_HANDLE position_handle, std::string& name);
bool gms_registry_get_size(REGISTRY_HANDLE position_handle,
                           size_t& width,
                           size_t& height);
int gms_main(int argc, const char* argv[]);
#endif