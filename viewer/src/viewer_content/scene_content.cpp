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
#include "viewer/viewer_content.hpp"

constexpr int REQUEST_DEFAULT_SCENE = -1;

class SceneContent : public ViewerContent {
    // ImGui wants raw pointers to names, but our public API returns
    // names as strings (by value), so we cache these names each time we
    // load a file
    std::vector<std::string> animationNames;
    std::vector<std::string> stateMachineNames;

    void loadNames(const rive::Artboard* ab) {
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

    std::string m_Filename;
    std::unique_ptr<rive::File> m_File;

    std::unique_ptr<rive::ArtboardInstance> m_ArtboardInstance;
    std::unique_ptr<rive::Scene> m_CurrentScene;
    int m_AnimationIndex = 0;
    int m_StateMachineIndex = -1;

    int m_width = 0, m_height = 0;
    rive::Mat2D m_InverseViewTransform;

    void initStateMachine(int index) {
        m_StateMachineIndex = -1;
        m_AnimationIndex = -1;
        m_CurrentScene = nullptr;
        m_ArtboardInstance = nullptr;

        m_ArtboardInstance = m_File->artboardDefault();
        m_ArtboardInstance->advance(0.0f);
        loadNames(m_ArtboardInstance.get());

        if (index < 0) {
            m_CurrentScene = m_ArtboardInstance->defaultStateMachine();
            index = m_ArtboardInstance->defaultStateMachineIndex();
        }
        if (!m_CurrentScene) {
            if (index >= m_ArtboardInstance->stateMachineCount()) {
                index = 0;
            }
            m_CurrentScene = m_ArtboardInstance->stateMachineAt(index);
        }
        if (!m_CurrentScene) {
            index = -1;
            m_CurrentScene = m_ArtboardInstance->animationAt(0);
            m_AnimationIndex = 0;
        }
        m_StateMachineIndex = index;

        if (m_CurrentScene) {
            m_CurrentScene->inputCount();
        }

        DumpCounters("After loading file");
    }

    void initAnimation(int index) {
        m_StateMachineIndex = -1;
        m_AnimationIndex = -1;
        m_CurrentScene = nullptr;
        m_ArtboardInstance = nullptr;

        m_ArtboardInstance = m_File->artboardDefault();
        m_ArtboardInstance->advance(0.0f);
        loadNames(m_ArtboardInstance.get());

        if (index >= 0 && index < m_ArtboardInstance->animationCount()) {
            m_AnimationIndex = index;
            m_CurrentScene = m_ArtboardInstance->animationAt(index);
            m_CurrentScene->inputCount();
        }

        DumpCounters("After loading file");
    }

public:
    SceneContent(const char filename[], std::unique_ptr<rive::File> file) :
        m_Filename(filename), m_File(std::move(file)) {
        initStateMachine(REQUEST_DEFAULT_SCENE);
    }

    void handlePointerMove(float x, float y) override {
        auto pointer = m_InverseViewTransform * rive::Vec2D(x, y);
        if (m_CurrentScene) {
            m_CurrentScene->pointerMove(pointer);
        }
    }

    void handlePointerDown(float x, float y) override {
        auto pointer = m_InverseViewTransform * rive::Vec2D(x, y);
        if (m_CurrentScene) {
            m_CurrentScene->pointerDown(pointer);
        }
    }

    void handlePointerUp(float x, float y) override {
        auto pointer = m_InverseViewTransform * rive::Vec2D(x, y);
        if (m_CurrentScene) {
            m_CurrentScene->pointerUp(pointer);
        }
    }

    void handleResize(int width, int height) override {
        m_width = width;
        m_height = height;
    }

    void handleDraw(rive::Renderer* renderer, double elapsed) override {
        if (m_CurrentScene) {
            m_CurrentScene->advanceAndApply(elapsed);

            renderer->save();

            auto viewTransform = rive::computeAlignment(rive::Fit::contain,
                                                        rive::Alignment::center,
                                                        rive::AABB(0, 0, m_width, m_height),
                                                        m_CurrentScene->bounds());
            renderer->transform(viewTransform);
            // Store the inverse view so we can later go from screen to world.
            m_InverseViewTransform = viewTransform.invertOrIdentity();
            // post_mouse_event(artboard.get(), canvas->getTotalMatrix());

            m_CurrentScene->draw(renderer);
            renderer->restore();
        }
    }

    void handleImgui() override {
        if (m_ArtboardInstance != nullptr) {
            ImGui::Begin(m_Filename.c_str(), nullptr);
            if (ImGui::ListBox(
                    "Animations",
                    &m_AnimationIndex,
                    [](void* data, int index, const char** name) {
                        auto& names = *static_cast<std::vector<std::string>*>(data);
                        *name = names[index].c_str();
                        return true;
                    },
                    &animationNames,
                    animationNames.size(),
                    4))
            {
                m_StateMachineIndex = -1;
                initAnimation(m_AnimationIndex);
            }
            if (ImGui::ListBox(
                    "State Machines",
                    &m_StateMachineIndex,
                    [](void* data, int index, const char** name) {
                        auto& names = *static_cast<std::vector<std::string>*>(data);
                        *name = names[index].c_str();
                        return true;
                    },
                    &stateMachineNames,
                    stateMachineNames.size(),
                    4))
            {
                m_AnimationIndex = -1;
                initStateMachine(m_StateMachineIndex);
            }
            if (m_CurrentScene != nullptr) {

                ImGui::Columns(2);
                ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.6666);

                for (int i = 0; i < m_CurrentScene->inputCount(); i++) {
                    auto inputInstance = m_CurrentScene->input(i);

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

std::unique_ptr<ViewerContent> ViewerContent::Scene(const char filename[]) {
    auto bytes = LoadFile(filename);
    if (auto file = rive::File::import(rive::toSpan(bytes), RiveFactory())) {
        return std::make_unique<SceneContent>(filename, std::move(file));
    }
    return nullptr;
}