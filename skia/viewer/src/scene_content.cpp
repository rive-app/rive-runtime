/*
 * Copyright 2022 Rive
 */

#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/animation/state_machine_bool.hpp"
#include "rive/animation/state_machine_trigger.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/layout.hpp"
#include "rive/math/aabb.hpp"

#include "cg_skia_factory.hpp"
#include "skia_renderer.hpp"
#include "viewer_content.hpp"


// ImGui wants raw pointers to names, but our public API returns
// names as strings (by value), so we cache these names each time we
// load a file
std::vector<std::string> animationNames;
std::vector<std::string> stateMachineNames;

constexpr int REQUEST_DEFAULT_SCENE = -1;

#include <time.h>
double GetSecondsToday() {
    time_t m_time;
    time(&m_time);
    struct tm tstruct;
    gmtime_r(&m_time, &tstruct);

    int hours = tstruct.tm_hour - 4;
    if (hours < 0) {
        hours += 12;
    } else if (hours >= 12) {
        hours -= 12;
    }

    auto secs = (double)hours * 60 * 60 +
                (double)tstruct.tm_min * 60 +
                (double)tstruct.tm_sec;
//    printf("%d %d %d\n", tstruct.tm_sec, tstruct.tm_min, hours);
//    printf("%g %g %g\n", secs, secs/60, secs/60/60);
    return secs;
}

// We hold onto the file's bytes for the lifetime of the file, in case we want
// to change animations or state-machines, we just rebuild the rive::File from
// it.
std::vector<uint8_t> fileBytes;


static void loadNames(const rive::Artboard* ab) {
    animationNames.clear();
    stateMachineNames.clear();
    if (ab) {
        for (size_t i = 0; i < ab->animationCount(); ++i) {
            animationNames.push_back(ab->animationNameAt(i));
        }
        for (size_t i = 0; i < ab->stateMachineCount(); ++i) {
            stateMachineNames.push_back(ab->stateMachineNameAt(i));
        }
    }
}

class SceneContent : public ViewerContent {
    std::string m_filename;
    std::unique_ptr<rive::File> m_file;
    
    std::unique_ptr<rive::ArtboardInstance> m_artboardInstance;
    std::unique_ptr<rive::Scene> m_currentScene;
    int m_animationIndex = 0;
    int m_stateMachineIndex = -1;
    
    int m_width = 0, m_height = 0;
    rive::Vec2D m_lastPointer;
    rive::Mat2D m_inverseViewTransform;

    void initStateMachine(int index) {
        m_stateMachineIndex = -1;
        m_animationIndex = -1;
        m_currentScene = nullptr;
        m_artboardInstance = nullptr;

        m_artboardInstance = m_file->artboardDefault();
        m_artboardInstance->advance(0.0f);
        loadNames(m_artboardInstance.get());

        if (index < 0) {
            m_currentScene = m_artboardInstance->defaultStateMachine();
            index = m_artboardInstance->defaultStateMachineIndex();
        }
        if (!m_currentScene) {
            if (index >= m_artboardInstance->stateMachineCount()) {
                index = 0;
            }
            m_currentScene = m_artboardInstance->stateMachineAt(index);
        }
        if (!m_currentScene) {
            index = -1;
            m_currentScene = m_artboardInstance->animationAt(0);
            m_animationIndex = 0;
        }
        m_stateMachineIndex = index;

        if (m_currentScene) {
            m_currentScene->inputCount();
        }

        DumpCounters("After loading file");
    }

    void initAnimation(int index) {
        m_stateMachineIndex = -1;
        m_animationIndex = -1;
        m_currentScene = nullptr;
        m_artboardInstance = nullptr;

        m_artboardInstance = m_file->artboardDefault();
        m_artboardInstance->advance(0.0f);
        loadNames(m_artboardInstance.get());

        if (index >= 0 && index < m_artboardInstance->animationCount()) {
            m_animationIndex = index;
            m_currentScene = m_artboardInstance->animationAt(index);
            m_currentScene->inputCount();
        }

        DumpCounters("After loading file");
    }

public:
    SceneContent(const char filename[], std::unique_ptr<rive::File> file) :
        m_filename(filename),
        m_file(std::move(file))
    {
        initStateMachine(REQUEST_DEFAULT_SCENE);
    }

    void handlePointerMove(float x, float y) override {
        m_lastPointer = m_inverseViewTransform * rive::Vec2D(x, y);
        if (m_currentScene) {
            m_currentScene->pointerMove(m_lastPointer);
        }
    }
    void handlePointerDown() override {
        if (m_currentScene) {
            m_currentScene->pointerDown(m_lastPointer);
        }
    }
    void handlePointerUp() override {
        if (m_currentScene) {
            m_currentScene->pointerUp(m_lastPointer);
        }
    }

    void handleResize(int width, int height) override {
        m_width = width;
        m_height = height;
    }

    void handleDraw(SkCanvas* canvas, double elapsed) override {

        if (m_currentScene) {
            // See if we can "set the time" e.g. clock statemachine
            if (auto num = m_currentScene->getNumber("isTime")) {
                num->value(GetSecondsToday()/60/60);
            }

            m_currentScene->advanceAndApply(elapsed);

            rive::SkiaRenderer renderer(canvas);
            renderer.save();

            auto viewTransform = rive::computeAlignment(rive::Fit::contain,
                                                        rive::Alignment::center,
                                                        rive::AABB(0, 0, m_width, m_height),
                                                        m_currentScene->bounds());
            renderer.transform(viewTransform);
            // Store the inverse view so we can later go from screen to world.
            m_inverseViewTransform = viewTransform.invertOrIdentity();
            // post_mouse_event(artboard.get(), canvas->getTotalMatrix());

            m_currentScene->draw(&renderer);
            renderer.restore();
        }
    }

    void handleImgui() override {
        if (m_artboardInstance != nullptr) {
            ImGui::Begin(m_filename.c_str(), nullptr);
            if (ImGui::ListBox(
                    "Animations",
                    &m_animationIndex,
                    [](void* data, int index, const char** name) {
                        *name = animationNames[index].c_str();
                        return true;
                    },
                    m_artboardInstance.get(),
                    animationNames.size(),
                    4))
            {
                m_stateMachineIndex = -1;
                initAnimation(m_animationIndex);
            }
            if (ImGui::ListBox(
                    "State Machines",
                    &m_stateMachineIndex,
                    [](void* data, int index, const char** name) {
                        *name = stateMachineNames[index].c_str();
                        return true;
                    },
                    m_artboardInstance.get(),
                    stateMachineNames.size(),
                    4))
            {
                m_animationIndex = -1;
                initStateMachine(m_stateMachineIndex);
            }
            if (m_currentScene != nullptr) {

                ImGui::Columns(2);
                ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.6666);

                for (int i = 0; i < m_currentScene->inputCount(); i++) {
                    auto inputInstance = m_currentScene->input(i);

                    if (inputInstance->input()->is<rive::StateMachineNumber>()) {
                        // ImGui requires names as id's, use ## to hide the
                        // label but still give it an id.
                        char label[256];
                        snprintf(label, 256, "##%u", i);

                        auto number = static_cast<rive::SMINumber*>(inputInstance);
                        float v = number->value();
                        ImGui::InputFloat(label, &v, 1.0f, 2.0f, "%.3f");
                        number->value(v);
                        ImGui::NextColumn();
                    } else if (inputInstance->input()->is<rive::StateMachineTrigger>()) {
                        // ImGui requires names as id's, use ## to hide the
                        // label but still give it an id.
                        char label[256];
                        snprintf(label, 256, "Fire##%u", i);
                        if (ImGui::Button(label)) {
                            auto trigger = static_cast<rive::SMITrigger*>(inputInstance);
                            trigger->fire();
                        }
                        ImGui::NextColumn();
                    } else if (inputInstance->input()->is<rive::StateMachineBool>()) {
                        // ImGui requires names as id's, use ## to hide the
                        // label but still give it an id.
                        char label[256];
                        snprintf(label, 256, "##%u", i);
                        auto boolInput = static_cast<rive::SMIBool*>(inputInstance);
                        bool value = boolInput->value();

                        ImGui::Checkbox(label, &value);
                        boolInput->value(value);
                        ImGui::NextColumn();
                    }
                    ImGui::Text("%s", inputInstance->input()->name().c_str());
                    ImGui::NextColumn();
                }

                ImGui::Columns(1);
            }
            ImGui::End();

        } else {
            ImGui::Text("Drop a .riv file to preview.");
        }
    }
};

rive::CGSkiaFactory skiaFactory;

std::unique_ptr<ViewerContent> ViewerContent::Scene(const char filename[]) {
    auto bytes = LoadFile(filename);
    if (auto file = rive::File::import(rive::toSpan(bytes), &skiaFactory)) {
        return std::make_unique<SceneContent>(filename, std::move(file));
    }
    return nullptr;
}