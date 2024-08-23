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
#include "rive/assets/image_asset.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "viewer/viewer_content.hpp"
#include "rive/relative_local_asset_loader.hpp"

#ifdef RIVE_RENDERER_TESS
#include "viewer/sample_tools/sample_atlas_packer.hpp"
#endif

constexpr int REQUEST_DEFAULT_SCENE = -1;

class SceneContent : public ViewerContent
{
    // ImGui wants raw pointers to names, but our public API returns
    // names as strings (by value), so we cache these names each time we
    // load a file
    std::vector<std::string> artboardNames;
    std::vector<std::string> animationNames;
    std::vector<std::string> stateMachineNames;

    void loadArtboardNames()
    {
        if (m_File)
        {
            artboardNames.clear();
            auto abCnt = m_File->artboardCount();

            for (int i = 0; i < abCnt; i++)
            {
                auto abName = m_File->artboardNameAt(i);
                artboardNames.push_back(abName);
            }
        }
    }

    void loadNames(const rive::Artboard* ab)
    {
        animationNames.clear();
        stateMachineNames.clear();
        if (ab)
        {
            for (size_t i = 0; i < ab->animationCount(); ++i)
            {
                animationNames.push_back(ab->animationNameAt(i));
            }
            for (size_t i = 0; i < ab->stateMachineCount(); ++i)
            {
                stateMachineNames.push_back(ab->stateMachineNameAt(i));
            }
        }
    }

    std::string m_Filename;
    std::unique_ptr<rive::File> m_File;

    std::unique_ptr<rive::ArtboardInstance> m_ArtboardInstance;
    std::unique_ptr<rive::Scene> m_CurrentScene;
    rive::ViewModelInstance* m_ViewModelInstance;
    int m_ArtboardIndex = 0;
    int m_AnimationIndex = 0;
    int m_StateMachineIndex = -1;

    int m_width = 0, m_height = 0;
    rive::Mat2D m_InverseViewTransform;

    void initArtboard(int index)
    {
        if (!m_File)
            return;
        loadArtboardNames();
        m_ArtboardInstance = nullptr;

        m_ArtboardIndex = (index == REQUEST_DEFAULT_SCENE) ? 0 : index;
        m_ArtboardInstance = m_File->artboardAt(m_ArtboardIndex);
        // m_ViewModelInstance = m_File->viewModelInstanceNamed("vm-3");
        m_ViewModelInstance = m_File->createViewModelInstance(m_ArtboardInstance.get());
        m_ArtboardInstance->dataContextFromInstance(m_ViewModelInstance);

        m_ArtboardInstance->advance(0.0f);
        loadNames(m_ArtboardInstance.get());

        initStateMachine(REQUEST_DEFAULT_SCENE);
    }

    void initStateMachine(int index)
    {
        m_StateMachineIndex = -1;
        m_AnimationIndex = -1;
        m_CurrentScene = nullptr;

        m_ArtboardInstance->advance(0.0f);

        if (index < 0)
        {
            m_CurrentScene = m_ArtboardInstance->defaultStateMachine();
            index = m_ArtboardInstance->defaultStateMachineIndex();
        }
        if (!m_CurrentScene)
        {
            if (index >= m_ArtboardInstance->stateMachineCount())
            {
                index = 0;
            }
            m_CurrentScene = m_ArtboardInstance->stateMachineAt(index);
        }
        if (!m_CurrentScene)
        {
            index = -1;
            m_CurrentScene = m_ArtboardInstance->animationAt(0);
            m_AnimationIndex = 0;
        }
        m_StateMachineIndex = index;

        if (m_CurrentScene)
        {
            m_CurrentScene->inputCount();
        }
    }

    void initAnimation(int index)
    {
        m_StateMachineIndex = -1;
        m_AnimationIndex = -1;
        m_CurrentScene = nullptr;

        m_ArtboardInstance->advance(0.0f);

        if (index >= 0 && index < m_ArtboardInstance->animationCount())
        {
            m_AnimationIndex = index;
            m_CurrentScene = m_ArtboardInstance->animationAt(index);
            m_CurrentScene->inputCount();
        }
    }

public:
    SceneContent(const char filename[], std::unique_ptr<rive::File> file) :
        m_Filename(filename), m_File(std::move(file))
    {
        initArtboard(REQUEST_DEFAULT_SCENE);
    }

    void handlePointerMove(float x, float y) override
    {
        auto pointer = m_InverseViewTransform * rive::Vec2D(x, y);
        if (m_CurrentScene)
        {
            m_CurrentScene->pointerMove(pointer);
        }
    }

    void handlePointerDown(float x, float y) override
    {
        auto pointer = m_InverseViewTransform * rive::Vec2D(x, y);
        if (m_CurrentScene)
        {
            m_CurrentScene->pointerDown(pointer);
        }
    }

    void handlePointerUp(float x, float y) override
    {
        auto pointer = m_InverseViewTransform * rive::Vec2D(x, y);
        if (m_CurrentScene)
        {
            m_CurrentScene->pointerUp(pointer);
        }
    }

    void handleResize(int width, int height) override
    {
        m_width = width;
        m_height = height;
    }

    void handleDraw(rive::Renderer* renderer, double elapsed) override
    {
        renderer->save();

        auto viewTransform = rive::computeAlignment(rive::Fit::contain,
                                                    rive::Alignment::center,
                                                    rive::AABB(0, 0, m_width, m_height),
                                                    m_ArtboardInstance->bounds());
        renderer->transform(viewTransform);
        // Store the inverse view so we can later go from screen to world.
        m_InverseViewTransform = viewTransform.invertOrIdentity();

        if (m_CurrentScene)
        {
            m_CurrentScene->advanceAndApply(elapsed);
            m_CurrentScene->draw(renderer);
        }
        else
        {
            m_ArtboardInstance->draw(renderer); // we're just a still-frame file/artboard
        }

        renderer->restore();
    }

#ifndef RIVE_SKIP_IMGUI
    void handleImgui() override
    {
        // For now the atlas packer only works with tess as it compiles in our
        // Bitmap decoder.
#ifdef RIVE_RENDERER_TESS
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Tools"))
            {
                if (ImGui::MenuItem("Build Atlas"))
                {
                    // Create an atlas packer.
                    rive::SampleAtlasPacker atlasPacker(2048, 2048);

                    // Have it pack the riv file, note that we need to re-load
                    // the file as the packer internally sets up a custom
                    // factory to process the images.
                    auto rivFileBytes = LoadFile(m_Filename.c_str());
                    atlasPacker.pack(rivFileBytes);

                    // The packer now contains the new atlas(es) and metadata
                    // (which image is in which atlas and the transform to apply
                    // to the UV coordiantes). This is where you'd probably
                    // serialize these results to new images and some metadata
                    // format containing the atlas locations.

                    // But for this demo, we're just going to pass this data on
                    // to the asset resolver used to load the file with no
                    // in-line images.

                    // On that note, let's strip the images.
                    rive::ImportResult stripResult;
                    auto strippedBytes = rive::File::stripAssets(rivFileBytes,
                                                                 {rive::ImageAsset::typeKey},
                                                                 &stripResult);
                    if (stripResult != rive::ImportResult::success)
                    {
                        printf("Failed to strip images\n");
                        return;
                    }

                    // We let the atlas packer handle loading the riv file using
                    // the previously generated assets. Note that we're loading
                    // our riv file with the images stripped out of it.
                    rive::ImportResult loadAtlasedResult;

                    rive::SampleAtlasLoader resolver(&atlasPacker);
                    if (auto file = rive::File::import(strippedBytes,
                                                       RiveFactory(),
                                                       &loadAtlasedResult,
                                                       &resolver))
                    {
                        m_File = std::move(file);
                        initArtboard(REQUEST_DEFAULT_SCENE);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
#endif
        if (m_ArtboardInstance != nullptr)
        {
            ImGui::Begin(m_Filename.c_str(), nullptr);
            if (ImGui::ListBox(
                    "Artboard",
                    &m_ArtboardIndex,
                    [](void* data, int index, const char** name) {
                        auto& names = *static_cast<std::vector<std::string>*>(data);
                        *name = names[index].c_str();
                        return true;
                    },
                    &artboardNames,
                    artboardNames.size(),
                    4))
            {
                initArtboard(m_ArtboardIndex);
            }
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
            if (m_CurrentScene != nullptr)
            {

                ImGui::Columns(2);
                ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.6666);

                for (int i = 0; i < m_CurrentScene->inputCount(); i++)
                {
                    auto inputInstance = m_CurrentScene->input(i);

                    if (inputInstance->input()->is<rive::StateMachineNumber>())
                    {
                        // ImGui requires names as id's, use ## to hide the
                        // label but still give it an id.
                        char label[256];
                        snprintf(label, 256, "##%u", i);

                        auto number = static_cast<rive::SMINumber*>(inputInstance);
                        float v = number->value();
                        ImGui::InputFloat(label, &v, 1.0f, 2.0f, "%.3f");
                        number->value(v);
                        ImGui::NextColumn();
                    }
                    else if (inputInstance->input()->is<rive::StateMachineTrigger>())
                    {
                        // ImGui requires names as id's, use ## to hide the
                        // label but still give it an id.
                        char label[256];
                        snprintf(label, 256, "Fire##%u", i);
                        if (ImGui::Button(label))
                        {
                            auto trigger = static_cast<rive::SMITrigger*>(inputInstance);
                            trigger->fire();
                        }
                        ImGui::NextColumn();
                    }
                    else if (inputInstance->input()->is<rive::StateMachineBool>())
                    {
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
        }
        else
        {
            ImGui::Text("Drop a .riv file to preview.");
        }
    }
#endif
};

std::unique_ptr<ViewerContent> ViewerContent::Scene(const char filename[])
{
    auto bytes = LoadFile(filename);
    rive::RelativeLocalAssetLoader loader(filename);
    rive::ImportResult result;
    if (auto file = rive::File::import(bytes, RiveFactory(), &result, &loader))
    {
        return rivestd::make_unique<SceneContent>(filename, std::move(file));
    }
    return nullptr;
}
