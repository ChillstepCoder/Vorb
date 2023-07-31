#include "stdafx.h"
#include "ParticleSystemEditorViewportPanel.h"

#include "resources/ResourceManager.h"
#include "resources/ParticleSystemRepository.h"

#include "camera/SimpleCamera.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

bool ParticleSystemEditorViewportPanel::updateAndRender()
{

    bool isOpen = true;
    ImGui::Begin("Fishing Editor", &isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

    ImVec2 mouseDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);

    f32v2 viewportDims = f32v2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
    updateCamera(viewportDims.x / viewportDims.y);

    mClearColor = f32v4(0.0f);

    //updateFramebufferAndLazyInit(imageDims);

    //clearFramebuffers();

    // Lazy init so we don't use GPU memory when not in editor
    if (sGBuffers[0] == nullptr) {
        initGBuffers(viewportDims);
    }

    renderCenterPanel(nullptr);

    ImGui::End();

    return isOpen;
}

void ParticleSystemEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize) {

    // TODO: UI Utilities
        // Helper for selected button styling
#define PUSH_COLOR(button, hover, active) \
    ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)button); \
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)hover); \
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)active);
#define PUSH_SELECTED_STYLE() \
    ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(1.0f, 0.6f, 0.6f)); \
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(1.0f, 0.7f, 0.7f)); \
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(1.0f, 0.8f, 0.8f));
#define POP_COLOR() ImGui::PopStyleColor(3);
#define SELECTED_BUTTON(b) PUSH_SELECTED_STYLE(); (b); POP_COLOR();

    ImGui::Spacing();
    // somewhere in your main window
    if (ImGui::Button("Add New System"))
    {
        ImGui::OpenPopup("SystemModal");
        strcpy_s(mTextInputBuffer, "System");
    }

    const ImVec2 contentAvail = ImGui::GetContentRegionAvail();

    bool is_open = true;

    if (ImGui::BeginPopupModal("SystemModal", &is_open))
    {
        ImGui::InputText("Name", mTextInputBuffer, TEXT_INPUT_SIZE);
        // Your popup content here
        if (ImGui::Button("Create"))
        {
            ImGui::CloseCurrentPopup();
            mSystemDef = Services::ResourceManager::ref().getParticleSystemRepository().tryAddNewParticleSystem(nString(mTextInputBuffer));
            if (!mSystemDef) {
                LOG_CRITICAL("Failed to create system {}", mTextInputBuffer);
            }
            mSystemDef->mSystemName = mTextInputBuffer;
            mTextInputBuffer[0] = '\0';

        } else {
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
                mTextInputBuffer[0] = '\0';
            }
        }
        ImGui::EndPopup();
    }

    if (mSystemDef) {
        bool changed = false;

        ImGui::Separator();
        ImGui::Text("System: %s", mSystemDef->mSystemName.c_str());

        if (ImGui::Button("Add Emitter")) {
            ImGui::OpenPopup("EmitterModal");
            strcpy_s(mTextInputBuffer, "Emitter");
        }

        if (ImGui::BeginPopupModal("EmitterModal", &is_open))
        {
            ImGui::InputText("Name", mTextInputBuffer, TEXT_INPUT_SIZE);
            // Your popup content here
            if (ImGui::Button("Create")) {
                ImGui::CloseCurrentPopup();
                ParticleEmitterDef& newEmitterDef = mSystemDef->mEmitters.emplace_back();
                newEmitterDef.mEmitterName = mTextInputBuffer;
                mTextInputBuffer[0] = '\0';

            }
            else {
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                    mTextInputBuffer[0] = '\0';
                }
            }
            ImGui::EndPopup();
        }
        
        // Show all emitters
        ImGui::Separator();
        ImGui::Text("Emitters:");
        int i = 0;
        for (auto&& emitter : mSystemDef->mEmitters) {
            ImGui::PushID(i++);
            if (mSelectedEmitter == &emitter) {
                SELECTED_BUTTON(ImGui::Button(emitter.mEmitterName.c_str(), ImVec2(contentAvail.x, 0)));
            }
            else {
                if (ImGui::Button(emitter.mEmitterName.c_str(), ImVec2(contentAvail.x, 0))) {
                    mSelectedEmitter = &emitter;
                }
            }
            ImGui::PopID();
        }
    }
}

#define COLOR_BUTTON_START(xx, yy, zz, name) \
    PUSH_COLOR(ImColor(ImVec4(xx, yy, zz, 0.8f)), ImColor(ImVec4(xx, yy, zz, 0.9f)), ImColor(ImVec4(xx, yy, zz, 1.0f))); \
    if (ImGui::Button(name, ImVec2(contentAvail.x - size, size))) {
#define COLOR_BUTTON_END() \
    } \
    POP_COLOR(); \
    ImGui::SameLine();

bool ParticleSystemEditorViewportPanel::updateAndRenderSecondaryControls(f32 ySize) {
    ImGui::BeginChild("Particle Emitter Editor", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);

    auto displayModuleSelectorCombo = [&](ParticleEmitterModuleStage stageBit) -> int {
        int itemCount = 0;
        const char* items[256];
        static int itemSelected = -1; // If the selection isn't within 0..count, Combo won't display a preview
        for (auto&& module : Services::ResourceManager::ref().getParticleSystemRepository().getEmitterModules()) {
            if (module->getStages().isBitSet(stageBit)) {
                items[itemCount++] = module->getName();
            }
        }
        ImGui::Combo("Select", &itemSelected, items, itemCount);
        return itemSelected;
    };

    if (mSelectedEmitter) {
        ImGui::Text("Emitter: %s", mSelectedEmitter->mEmitterName.c_str());
        ImGui::Separator();
        ImGui::Text("Modules");
        const ImVec2 contentAvail = ImGui::GetContentRegionAvail();
        constexpr int size = 17;
        
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
        COLOR_BUTTON_START(0.0f, 0.8f, 0.0f, "Emitter Update");
        COLOR_BUTTON_END();
        if (ImGui::Button("+", ImVec2(size, size))) {
            ImGui::OpenPopup("Emitter Update Module");
        }
        if (ImGui::BeginPopupModal("UpdateModules"))
        {
            if (displayModuleSelectorCombo(ParticleEmitterModuleStage::EmitterUpdate)) {
                ImGui::CloseCurrentPopup();
            }

            // Your popup content here
           /* if (ImGui::Button("Create")) {
                ImGui::CloseCurrentPopup();
                ParticleEmitterDef& newEmitterDef = mSystemDef->mEmitters.emplace_back();
                newEmitterDef.mEmitterName = mTextInputBuffer;
                mTextInputBuffer[0] = '\0';

            }
            else {
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                    mTextInputBuffer[0] = '\0';
                }
            }*/
            ImGui::EndPopup();
        }

        for (auto&& module : mSelectedEmitter->mEmitterUpdateModules) {
            if (ImGui::Button(module->getName(), ImVec2(ImGui::GetContentRegionAvail().x, size))) {
                mSelectedModule = module.get();
            }
        }

        ImGui::PopStyleVar(4);

        ImGui::Spacing();
        int height = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y;
        ImGui::Button("Emitter UpdateTest", ImVec2(contentAvail.x - height, height));
        ImGui::SameLine();
        ImGui::Button("+", ImVec2(height, height));
        if (ImGui::CollapsingHeader("Particle Init", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SameLine();
            ImGui::Button("+");

        }
        if (ImGui::CollapsingHeader("Particle Update", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SameLine();
            ImGui::Button("+");

        }
    }
    else {
        ImGui::Text("Select a Particle Emitter");
    }
    ImGui::EndChild();
    return true;
}

bool ParticleSystemEditorViewportPanel::updateAndRenderTertiaryControls(f32 ySize) {
    ImGui::BeginChild("Particle Module Editor", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    if (mSelectedModule) {
        ImGui::Text("Module: %s", mSelectedModule->getName());
    }
    else {
        ImGui::Text("Select a Module");
    }
    ImGui::EndChild();
    return true;
}

void ParticleSystemEditorViewportPanel::renderMesh() {
    renderGrid(camera->getViewProjectionMatrix());
}

void ParticleSystemEditorViewportPanel::setParticleSystemDef(ParticleSystemDef* systemDef) {
    mSystemDef = systemDef;
}
