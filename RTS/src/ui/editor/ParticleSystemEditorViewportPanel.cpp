#include "stdafx.h"
#include "ParticleSystemEditorViewportPanel.h"

#include "resources/ResourceManager.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/MaterialRenderer.h"
#include "resources/ParticleSystemRepository.h"

#include "rendering/particle/CPUParticleSystem.h"

#include "camera/SimpleCamera.h"

#include "ui/ImguiUtil.hpp"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

// TODO: UI Utilities
    // Helper for selected button styling
#define PUSH_COLOR(button, hover, active) \
    ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)button); \
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)hover); \
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)active);
#define PUSH_SELECTED_STYLE() \
    ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.6f, 0.9f, 0.7f)); \
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.75f, 0.9f, 0.7f)); \
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.8f, 0.9f, 0.8f));
#define POP_COLOR() ImGui::PopStyleColor(3);
#define SELECTED_BUTTON(b) PUSH_SELECTED_STYLE(); (b); POP_COLOR();

ParticleSystemEditorViewportPanel::ParticleSystemEditorViewportPanel() : IEditorViewportPanel() {

}

ParticleSystemEditorViewportPanel::~ParticleSystemEditorViewportPanel() {

}

bool ParticleSystemEditorViewportPanel::updateAndRender(f32 elapsedSec) {
    mCurrentElapsedSec = elapsedSec;

    // Refresh asset every frame in case awaiting load
    if (mSystemDefHandle) {
        mSystemDef = mSystemDefHandle->editorTryGetMutableAsset();
    }

    mCurrentTime += elapsedSec;
    if (mCurrentTime >= mTimelineEnd) {
        createPreviewSystem();
    }

    bool isOpen = true;
    ImGui::Begin("Particle System Editor", &isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);

    ImVec2 mouseDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);

    f32v2 viewportDims = f32v2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
    updateCamera(viewportDims.x / viewportDims.y);

    mClearColor = f32v4(0.3f, 0.3f, 0.3f, 1.0f);

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


    // Add FPS for convenience
    char buffer[64];
    sprintf_s(buffer, sizeof(buffer), "FPS: %.0f", sFps);
    ImGui::Text(buffer);

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
            mSystemDefHandle = ParticleSystemRepository::get().editorTryAddNewAsset(StrToken(mTextInputBuffer));
            if (!mSystemDefHandle) {
                LOG_CRITICAL("Failed to create system {}", mTextInputBuffer);
            }
            else {
                mSystemDef = mSystemDefHandle->editorTryGetMutableAsset();
                mSystemDef->setName(StrToken(mTextInputBuffer));
                ParticleEmitterDef& defaultEmitter = mSystemDef->mEmitters.emplace_back();
                defaultEmitter.mEmitterName = "DefaultEmitter";
                defaultEmitter.mDefaultMaterialID = ParticleSystemRepository::get().getDefaultMaterialID();
                defaultEmitter.mShader = Services::ResourceManager::ref().getMaterialShaderManager().getMaterialShader("textured_particle_3d_bb");
                mSelectedEmitter = &defaultEmitter;
                mTextInputBuffer[0] = '\0';
            }

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
        ImGui::Text("System: %s", mSystemDef->getName().toString().c_str());
        if (ImGui::Button("Rename")) {
            mRenamePopup = std::make_unique<ImguiUtil::RenameAssetPopup>(mSystemDef->getName().toString(), (void*)mSystemDef);
        }
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            if (!ParticleSystemRepository::get().saveAsset(mSystemDef->getID())) {
                pError("FAILED TO SAVE PARTICLE SYSTEM!");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
            mConfirmDeletePopup = std::make_unique<ImguiUtil::ConfirmDeletePopup>(mSystemDef->getName().toString(), (void*)mSystemDef);
        }
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button("Add Emitter")) {
            ImGui::OpenPopup("EmitterModal");
            strcpy_s(mTextInputBuffer, "Emitter");
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate Existing Emitter")) {
            openDuplicateEmitterPopup();
        }
        if (ImGui::BeginPopupModal("EmitterModal", &is_open)) {
            ImGui::InputText("Name", mTextInputBuffer, TEXT_INPUT_SIZE);
            // Your popup content here
            if (ImGui::Button("Create")) {
                ImGui::CloseCurrentPopup();
                ParticleEmitterDef& newEmitterDef = mSystemDef->mEmitters.emplace_back();
                newEmitterDef.mEmitterName = mTextInputBuffer;
                newEmitterDef.mDefaultMaterialID = ParticleSystemRepository::get().getDefaultMaterialID();
                newEmitterDef.mShader = Services::ResourceManager::ref().getMaterialShaderManager().getMaterialShader("textured_particle_3d_bb");
                mSelectedEmitter = &newEmitterDef;
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
        
        // Make sure we have emitter visibility
        if (mShowEmitters.size() != mSystemDef->mEmitters.size()) {
            mShowEmitters.resize(mSystemDef->mEmitters.size(), true);
        }

        // Show all emitters
        ImGui::Separator();
        ImGui::Text("Emitters:");
        int i = 0;
        for (auto&& emitter : mSystemDef->mEmitters) {
            ImGui::PushID(i);
            constexpr int VISIBILITY_SIZE = 32;
            if (mSelectedEmitter == &emitter) {
                SELECTED_BUTTON(ImGui::Button(emitter.mEmitterName.c_str(), ImVec2(contentAvail.x - VISIBILITY_SIZE, 0)));
            }
            else {
                if (ImGui::Button(emitter.mEmitterName.c_str(), ImVec2(contentAvail.x - VISIBILITY_SIZE, 0))) {
                    mSelectedEmitter = &emitter;
                }
            }
            ImGui::SameLine();
            if (mShowEmitters[i]) {
                PUSH_SELECTED_STYLE();
                if (ImGui::Button("O")) {
                    mShowEmitters[i] = false;
                }
                POP_COLOR();
            }
            else {
                PUSH_COLOR(ImColor::HSV(1.0f, 0.7f, 0.6f), ImColor::HSV(1.0f, 0.7f, 0.8f), ImColor::HSV(1.0f, 0.7f, 1.0f));
                if (ImGui::Button("x")) {
                    mShowEmitters[i] = true;
                }
                POP_COLOR();
            }
            ImGui::PopID();
            ++i;
        }

        // Popups are opened in this panel
        updatePopups();
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
    ImGui::BeginChild("Particle Emitter Editor", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings/* | ImGuiWindowFlags_NoScrollbar*/);

    auto displayModuleSelectorCombo = [&](ParticleEmitterModuleStage stageBit) -> const CPUParticleEmitterModule* {
        const auto& modules = yml::getAllObjects<CPUParticleEmitterModule>();
        for (auto&& iter : modules) {
            const std::unique_ptr<CPUParticleEmitterModule>& module = iter.second;
            if (module->getStages().isBitSet(stageBit)) {
                if (ImGui::Button(module->getName(), ImVec2(300, 0.0f))) {
                    return module.get();
                }
            }
        }
        ImGui::Spacing();
        return nullptr;
    };

 
    if (mSelectedEmitter) {
        ImGui::Text("Emitter: %s", mSelectedEmitter->mEmitterName.c_str());
        ImGui::Separator();
        ImGui::Text("Modules");
        const ImVec2 contentAvail = ImGui::GetContentRegionAvail();
        constexpr int size = 17;

        auto displayCategory = [&](const char* popupName, const char* categoryName, ParticleEmitterModuleStage stage, CPUParticleEmitterModuleVector& modules, f32 r, f32 g, f32 b) -> bool {
            bool selected = false;
            ImGui::PushID(categoryName);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
            COLOR_BUTTON_START(r, g, b, categoryName);
            COLOR_BUTTON_END();
            if (ImGui::Button("+", ImVec2(size, size))) {
                ImGui::OpenPopup(popupName);
            }

            int mid = 0;
            for (auto&& it = modules.begin(); it != modules.end();) {
                ImGui::PushID(++mid);
                auto& module = *it;
                if (mSelectedModule == module.get()) {
                    SELECTED_BUTTON(ImGui::Button(module->getName(), ImVec2(contentAvail.x - size, size)));
                }
                else {
                    if (ImGui::Button(module->getName(), ImVec2(contentAvail.x - size, size))) {
                        mSelectedModule = module.get();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("-", ImVec2(size, size))) {
                    if (mSelectedModule == module.get()) {
                        mSelectedModule = nullptr;
                    }
                    it = modules.erase(it);
                    // Refresh
                    createPreviewSystem();
                }
                else {
                    ++it;
                }
                ImGui::PopID();
            }

            ImGui::PopStyleVar(4);

            if (ImGui::BeginPopupModal(popupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                if (const CPUParticleEmitterModule* displayModule = displayModuleSelectorCombo(stage)) {
                    mSelectedModule = modules.emplace_back(displayModule->clone()).get();
                    selected = true;
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
            return selected;
        };
        
        if (displayCategory("Emitter Update Modules", "Emitter Update", ParticleEmitterModuleStage::EmitterUpdate, mSelectedEmitter->mModules.mEmitterUpdate, 0.0f, 0.8f, 0.0f)) {
            createPreviewSystem();
        }
        ImGui::Spacing();
        if (displayCategory("Particle Init Modules", "Particle Init", ParticleEmitterModuleStage::ParticleInit, mSelectedEmitter->mModules.mParticleInit, 0.7f, 0.7f, 0.0f)) {
            createPreviewSystem();
        }
        ImGui::Spacing();
        if (displayCategory("Particle Update Modules", "Particle Update", ParticleEmitterModuleStage::ParticleUpdate, mSelectedEmitter->mModules.mParticleUpdate, 0.8f, 0.0f, 0.0f)) {
            createPreviewSystem();
        }

    }
    else {
        ImGui::Text("Select a Particle Emitter");
    }

    if (!mSystemDef) {
        ImGui::Text("Select a Particle System");
        ImGui::EndChild();
        return true;
    }
    ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
    ImGui::Spacing(); ImGui::Separator();

    if (mSelectedEmitter) {
        ImGui::Text("Emitter: %s", mSelectedEmitter->mEmitterName.c_str());
        bool changed = false;
        changed |= ImGui::SliderFloat2("Scale", &mSelectedEmitter->mDefaultScale.x, 0.01f, 5.0f, "%.2f");

        color4& color = mSelectedEmitter->mDefaultColor;
        float colorf[4] = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f };
        changed |= ImGui::ColorEdit4("Color", colorf, ImGuiColorEditFlags_Uint8);
        color = color4((ui8)roundf(colorf[0] * 255.0f), (ui8)roundf(colorf[1] * 255.0f), (ui8)roundf(colorf[2] * 255.0f), (ui8)roundf(colorf[3] * 255.0f));

        int maxParticles = mSelectedEmitter->mMaxParticles;
        changed |= ImGui::SliderInt("Max Particles", &maxParticles, 1, 20000);
        mSelectedEmitter->mMaxParticles = maxParticles;

        changed |= ImGui::SliderFloat("Emitter Lifetime Sec", &mSelectedEmitter->mLifetimeSec, 0.01f, 50.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
        changed |= ImGui::SliderFloat("Particle Lifespan Sec", &mSelectedEmitter->mDefaultParticleLifespanSec, 0.01f, 50.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
        changed |= ImGui::Checkbox("Looping", &mSelectedEmitter->mLooping);

        if (changed) {
            createPreviewSystem();
        }

        //MaterialID mDefaultMaterialID = 0;

    }
    ImGui::EndChild();

    return true;
}

bool ParticleSystemEditorViewportPanel::updateAndRenderTertiaryControls(f32 ySize) {
    ImGui::BeginChild("Particle Module Editor", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);

    if (!mSystemDef) {
        ImGui::Text("Select a Particle System");
        ImGui::EndChild();
        return true;
    }

    ImGui::Spacing(); ImGui::Separator();
    if (mSelectedModule) {
        ImGui::Text("Module: %s", mSelectedModule->getName());
        if (mSelectedModule->updateAndRenderEditorControls()) {
            createPreviewSystem();
        }
    }
    else {
        ImGui::Text("Select a Module");
    }
    ImGui::EndChild();
    return true;
}

void ParticleSystemEditorViewportPanel::updateAndRenderBottomControls() {

    ImGui::Begin("Particle Bottom Panel", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar);

    if (mSystemDef) {
        ImGui::Text("System: %s Particles (Fragmentation): %d (%d)", mSystemDef->getName().toString().c_str(), mPreviewSystem ? mPreviewSystem->getNumParticles() : 0, mPreviewSystem ? mPreviewSystem->getFragmentation() : 0);
        ImGui::SliderFloat("Preview Time", &mTimelineEnd, 0.0f, 20.0);
        f32 time = mCurrentTime;
        ImGui::SliderFloat("Time", &time, 0.0f, mTimelineEnd);
        ImGui::Spacing(); ImGui::Separator();
    }

    mBottomHeight = ImGui::GetWindowSize().y;
    ImGui::End();
}

void ParticleSystemEditorViewportPanel::renderMesh() {
    renderGrid(camera->getViewProjectionMatrix());

    // Render preview system
    if (mPreviewSystem) {
        mPreviewSystem->updateAndRenderEditor(mCurrentElapsedSec, camera->getViewProjectionMatrix(), mShowEmitters);
    }
}

void ParticleSystemEditorViewportPanel::setParticleSystemDef(AssetID systemId) {
    mSystemDefHandle = ParticleSystemRepository::get().getAssetHandle(systemId);
    if (systemIsLoaded()) {
        mSystemDef = mSystemDefHandle->editorTryGetMutableAsset();
        mSelectedEmitter = &mSystemDef->mEmitters[0];
    }
    else {
        mSelectedEmitter = nullptr;
    }
    createPreviewSystem();
}

void ParticleSystemEditorViewportPanel::createPreviewSystem() {
    if (!mSystemDef) return;
    mCurrentTime = 0.0f;
    mPreviewSystem = std::make_unique<CPUParticleSystem>(*mSystemDef);
}

void ParticleSystemEditorViewportPanel::updatePopups() {
    if (mRenamePopup) {
        if (mRenamePopup->updateAndRender()) {
            const nString& result = mRenamePopup->getResult();
            assert(false);
            mRenamePopup.reset();
        }
    }
    else if (mConfirmDeletePopup) {
        if (mConfirmDeletePopup->updateAndRender()) {
            bool result = mConfirmDeletePopup->getResult();
            mConfirmDeletePopup.reset();
            if (result && mSystemDef) {
                ParticleSystemRepository::get().deleteAsset(mSystemDef->getID());
                mSystemDef = nullptr;
                mSelectedEmitter = nullptr;
            }
        }
    }
    else if (mDuplicateObjectPopup) {
        if (mDuplicateObjectPopup->updateAndRender()) {
            duplicateGlobalEmitter(mDuplicateObjectPopup->getResultName());
            mDuplicateObjectPopup.reset();
        }
    }
}

void ParticleSystemEditorViewportPanel::openDuplicateEmitterPopup() {
    std::vector<nString> emitterNames;
    nString name; // Share memory
    ParticleSystemRepository::get().forEachRegisteredAsset([&](IAssetRepository<ParticleSystemDef>& repo, ParticleSystemDef* def, const AssetRegistryEntry& entry) {
        if (def) {
            name = def->getName().toString();
            for (auto& emitter : def->mEmitters) {
                emitterNames.push_back(name + "." + emitter.mEmitterName);
            }
        }
        else {
            // Request asset load if needed
            if (!mAssetHandleBundle.hasAssetHandle(entry.mID, repo.getAssetType())) {
                mAssetHandleBundle.addAssetHandle(repo.getAssetHandle(entry.mID));
            }
        }
        return false;
    });
    mDuplicateObjectPopup = std::make_unique<ImguiUtil::CustomSelectorPopup>(emitterNames);
}

void ParticleSystemEditorViewportPanel::duplicateGlobalEmitter(const nString& emitterName) {
    if (emitterName.empty()) return;
    if (mSystemDef == nullptr) return;

    nString name; // Share memory
    ParticleSystemRepository::get().forEachLoadedAsset([&](IAssetRepository<ParticleSystemDef>& repo, ParticleSystemDef& def) {
        name = def.getName().toString();
        for (auto& emitter : def.mEmitters) {
            if (emitterName == name + "." + emitter.mEmitterName) {
                mSystemDef->mEmitters.emplace_back(emitter);
                mSelectedEmitter = &mSystemDef->mEmitters.back();
                createPreviewSystem();
                return true;
            }
        }
        return false;
    });
}
