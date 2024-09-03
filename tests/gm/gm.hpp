/*
 * Copyright 2022 Rive
 */

#ifndef _RIVEGM_GM_HPP_
#define _RIVEGM_GM_HPP_

#include "rive/renderer.hpp"
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace rivegm
{

class GM
{
    std::string m_Name;
    const int m_Width, m_Height;
    bool m_InBenchMode = false;

public:
    GM(int width, int height, const char name[]) :
        m_Name(name, strlen(name)), m_Width(width), m_Height(height)
    {}
    virtual ~GM() {}

    int width() const { return m_Width; }
    int height() const { return m_Height; }
    std::string name() const { return m_Name; }

    void onceBeforeDraw() { this->onOnceBeforeDraw(); }

    // Calls clearColor(), TestingWindow::beginFrame(), draw(), TestingWindow::flush().
    // (Most GMs just need to override onDraw() instead of overriding this method.)
    virtual void run(std::vector<uint8_t>* pixels);

    virtual rive::ColorInt clearColor() const { return 0xffffffff; }

    void draw(rive::Renderer*);

    int benchLoopCount() { return this->onBenchLoopCount(); }

    bool benchMode() const { return m_InBenchMode; }

    void setBenchMode(bool b) { m_InBenchMode = b; }

protected:
    virtual int onBenchLoopCount() { return 1; }
    virtual void onOnceBeforeDraw() {}
    virtual void onDraw(rive::Renderer*) = 0;
};

template <typename T> class Registry
{
    static Registry* s_Head;

    T m_Value;
    Registry* m_Next;

public:
    Registry(T value) : m_Value(value)
    {
        m_Next = s_Head;
        s_Head = this;
    }

    static const Registry* head() { return s_Head; }
    const T& get() const { return m_Value; }
    const Registry* next() const { return m_Next; }
};

template <typename T> Registry<T>* Registry<T>::s_Head;

using GMFactory = std::unique_ptr<GM> (*)();
using GMRegistry = Registry<GMFactory>;

} // namespace rivegm

// Macro helpers (lifted from SkMacros.h

#define RIVE_MACRO_CONCAT(X, Y) RIVE_MACRO_CONCAT_IMPL_PRIV(X, Y)
#define RIVE_MACRO_CONCAT_IMPL_PRIV(X, Y) X##Y

#define RIVE_MACRO_APPEND_COUNTER(name) RIVE_MACRO_CONCAT(name, __COUNTER__)

// Usage: GMREGISTER( return new mygmclass(...) )
//
#define GMREGISTER(code)                                                                           \
    static GMRegistry RIVE_MACRO_APPEND_COUNTER(rivegm_registry)(                                  \
        []() { return std::unique_ptr<rivegm::GM>([]() { code; }()); });

// Usage:
//
//   DEF_SIMPLE_GM(name, width, height, renderer) {
//       renderer->...
//   }
//
#define DEF_SIMPLE_GM(NAME, WIDTH, HEIGHT, RENDERER)                                               \
    class NAME##_GM : public rivegm::GM                                                            \
    {                                                                                              \
    public:                                                                                        \
        NAME##_GM() : GM(WIDTH, HEIGHT, #NAME) {}                                                  \
        void onDraw(rive::Renderer*) override;                                                     \
    };                                                                                             \
    GMREGISTER(return new NAME##_GM)                                                               \
    void NAME##_GM::onDraw(rive::Renderer* RENDERER)

// Usage:
//
//   DEF_SIMPLE_GM_WITH_CLEAR_COLOR(name, clearColor, width, height, renderer) {
//       renderer->...
//   }
//
#define DEF_SIMPLE_GM_WITH_CLEAR_COLOR(NAME, CLEAR_COLOR, WIDTH, HEIGHT, RENDERER)                 \
    class NAME##_GM : public rivegm::GM                                                            \
    {                                                                                              \
    public:                                                                                        \
        NAME##_GM() : GM(WIDTH, HEIGHT, #NAME) {}                                                  \
        ColorInt clearColor() const override { return CLEAR_COLOR; }                               \
        void onDraw(rive::Renderer*) override;                                                     \
    };                                                                                             \
    GMREGISTER(return new NAME##_GM)                                                               \
    void NAME##_GM::onDraw(rive::Renderer* RENDERER)

#endif
