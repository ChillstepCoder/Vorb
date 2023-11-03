#include "stdafx.h"
#include "ParticleSystemEditorViewportPanel.h"

#include "resources/ResourceManager.h"
#include "rendering/MaterialShaderRepository.h"
#include "resources/MaterialRepository.h"
#include "resources/TextureRepository.h"
#include "rendering/MaterialRenderer.h"
#include "resources/ParticleSystemRepository.h"

#include "rendering/particle/CPUParticleSystem.h"

#include "camera/SimpleCamera.h"

#include "ui/UIContext.h"
#include "ui/ImguiUtil.hpp"
#include "ui/editor/ImguiAssetThumbnails.h"

#include <imgui.h>
#include <imgui_internal.h>


#include <extern/ImGuiFileDialog/ImGuiFileDialog.h>

const nString SAVE_DIALOG_NAME = "SaveFileDialog";

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

ParticleSystemEditorViewportPanel::ParticleSystemEditorViewportPanel() {

}

ParticleSystemEditorViewportPanel::~ParticleSystemEditorViewportPanel() {

}

void ParticleSystemEditorViewportPanel::updateAndRenderInternal(f32 elapsedSec) {
    mCurrentTime += elapsedSec;

    if (mAssetWasChanged) {
        mAssetWasChanged = false;
        if (mAssetData) {
            mSelectedEmitter = &mAssetData->mEmitters[0];
        }
        createPreviewSystem();
    }

    if (!mAssetData) {
        mCurrentTime = mTimelineEnd;
        mSelectedEmitter = nullptr;
    }
    if (mCurrentTime >= mTimelineEnd) {
        createPreviewSystem();
    }
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
            mAssetHandle = ParticleSystemRepository::get().editorTryAddNewAsset(StrToken((const char*)mTextInputBuffer));
            if (!mAssetHandle) {
                LOG_CRITICAL("Failed to create system {}", mTextInputBuffer);
            }
            else {
                mAssetData = mAssetHandle->editorTryGetMutableAsset();
                mAssetData->setName(StrToken((const char*)mTextInputBuffer));
                ParticleEmitterDef& defaultEmitter = mAssetData->mEmitters.emplace_back();
                defaultEmitter.mEmitterName = CStrToken("default_emitter");
                defaultEmitter.mDefaultMaterialID = ParticleSystemRepository::get().getDefaultMaterialID();
                defaultEmitter.mShaderName = CStrToken("particle_bb_3d");
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

    if (mAssetData) {
        bool changed = false;

        ImGui::Separator();
        ImGui::Text("System: %s", mAssetData->getName().toString().c_str());
        if (ImGui::Button("Rename")) {
            mRenamePopup = std::make_unique<ImguiUtil::RenameAssetPopup>(mAssetData->getName().toString(), (void*)mAssetData);
        }
        ImGui::SliderFloat("Lifetime", &mAssetData->mLifetimeSec, 0.0f, 60.0f);
        if (ImGui::Button("Save")) {
            ParticleSystemRepository& repo = ParticleSystemRepository::get();
            if (repo.getAssetFilePath(mAssetData->getID()).isNull()) {
                ImGuiFileDialog::Instance()->OpenDialog(SAVE_DIALOG_NAME, "Save As", ".psys", "./data/particle/" + mAssetData->getName().toString(), 1, nullptr, ImGuiFileDialogFlags_Modal);
            }
            else {
                if (!repo.saveAsset(mAssetData->getID())) {
                    panic("FAILED TO SAVE PARTICLE SYSTEM {}", repo.getAssetFilePath(mAssetData->getID()).getCString());
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
            mConfirmDeletePopup = std::make_unique<ImguiUtil::ConfirmDeletePopup>(mAssetData->getName().toString(), (void*)mAssetData);
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
                ParticleEmitterDef& newEmitterDef = mAssetData->mEmitters.emplace_back();
                newEmitterDef.mEmitterName = StrToken(mTextInputBuffer);
                newEmitterDef.mDefaultMaterialID = ParticleSystemRepository::get().getDefaultMaterialID();
                newEmitterDef.mShaderName = CStrToken("particle_bb_3d");
                mSelectedEmitter = &newEmitterDef;
                mSelectedModule = nullptr;
                mSelectedModuleVector = nullptr;
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
        if (mShowEmitters.size() != mAssetData->mEmitters.size()) {
            mShowEmitters.resize(mAssetData->mEmitters.size(), true);
        }

        // Show all emitters
        ImGui::Separator();
        ImGui::Text("Emitters:");
        int i = 0;
        for (auto&& emitter : mAssetData->mEmitters) {
            ImGui::PushID(i);
            constexpr int VISIBILITY_SIZE = 32;
            if (mSelectedEmitter == &emitter) {
                SELECTED_BUTTON(ImGui::Button(emitter.mEmitterName.toString().c_str(), ImVec2(contentAvail.x - VISIBILITY_SIZE, 0)));
            }
            else {
                if (ImGui::Button(emitter.mEmitterName.toString().c_str(), ImVec2(contentAvail.x - VISIBILITY_SIZE, 0))) {
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
        ImGui::Text("Emitter: %s", mSelectedEmitter->mEmitterName.toString().c_str());
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
                        mSelectedModuleVector = &modules;
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("-", ImVec2(size, size))) {
                    if (mSelectedModule == module.get()) {
                        mSelectedModule = nullptr;
                        mSelectedModuleVector = nullptr;
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
                    mSelectedModuleVector = &modules;
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

    if (!mAssetData) {
        ImGui::Text("Select a Particle System");
        ImGui::EndChild();
        return true;
    }
    ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
    ImGui::Spacing(); ImGui::Separator();

    if (mSelectedEmitter) {
        ImGui::Text("Emitter: %s", mSelectedEmitter->mEmitterName.toString().c_str());
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
       
        if (ImGui::Button("Material")) {
            mAssetSelectorPopup = std::make_unique<ImguiUtil::AssetSelectorPopup>(MaterialRepository::get().getAssetRegistry());
            mAssetSelectorPopup->setThumbnailFunc(ImguiAssetThumbnails::getThumbnailFunction<MaterialDef>(), f32v2(50.0f));
        }
        if (mAssetSelectorPopup) {
            if (mAssetSelectorPopup->updateAndRender(UIContext::getWindowDims().y * 0.9f)) {
                assert(mSelectedEmitter);
                mSelectedEmitter->mDefaultMaterialName = mAssetSelectorPopup->getResult().mName;
                mSelectedEmitter->mDefaultMaterialID = mAssetSelectorPopup->getResult().getId();
                mAssetSelectorPopup.reset();
                changed = true;
            }
        }

        ImGui::SameLine();
        ImGui::Text(mSelectedEmitter->mDefaultMaterialID != INVALID_MATERIAL_ID ? MaterialRepository::get().getAssetName(mSelectedEmitter->mDefaultMaterialID).toString().c_str() : "NONE");
        ImguiAssetThumbnails::getThumbnailFunction<MaterialDef>()(mSelectedEmitter->mDefaultMaterialID, f32v2(60.0f));

        changed |= ImGui::Checkbox("Looping", &mSelectedEmitter->mLooping);

        changed |= ImguiUtil::EnumCombo("Blend Mode", mSelectedEmitter->mBlendMode);

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

    if (!mAssetData) {
        ImGui::Text("Select a Particle System");
        ImGui::EndChild();
        return true;
    }

    ImGui::Spacing(); ImGui::Separator();
    if (mSelectedModule) {
        
        size_t moduleIndex;
        CPUParticleEmitterModuleVector& moduleVec = *mSelectedModuleVector;
        for (moduleIndex = 0; moduleIndex < moduleVec.size() &&
            moduleVec[moduleIndex].get() != mSelectedModule; ++moduleIndex) {}
        ImGui::Text("Module: %s", mSelectedModule->getName());
        // Up and down arrows
        if (moduleIndex == moduleVec.size() - 1) {
            ImGui::Button("x");
        } else if (ImGui::Button("v")) {
            std::swap(moduleVec[moduleIndex + 1], moduleVec[moduleIndex]);
        }
        ImGui::SameLine();
        if (moduleIndex == 0) {
            ImGui::Button("x");
        }
        else if (ImGui::Button("^")) {
            std::swap(moduleVec[moduleIndex - 1], moduleVec[moduleIndex]);
        }
        ImGui::Separator();
        

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

    ImGui::Begin("Bottom Controls", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavFocus);

    if (mAssetData) {
        ImGui::Text("System: %s Particles (Fragmentation): %d (%d)", mAssetData->getName().toString().c_str(), mPreviewSystem ? mPreviewSystem->getNumParticles() : 0, mPreviewSystem ? mPreviewSystem->getFragmentation() : 0);
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

void ParticleSystemEditorViewportPanel::createPreviewSystem() {
    if (!mAssetData) return;
    mCurrentTime = 0.0f;
    mPreviewSystem = std::make_unique<CPUParticleSystem>(*mAssetData, f32v3(0.0f));
}

void ParticleSystemEditorViewportPanel::updatePopups() {
    if (mRenamePopup) {
        if (mRenamePopup->updateAndRender()) {
            const nString& result = mRenamePopup->getResult();
            if (result.size()) {
                assert(false);
            }
            mRenamePopup.reset();
        }
    }
    else if (mConfirmDeletePopup) {
        if (mConfirmDeletePopup->updateAndRender()) {
            bool result = mConfirmDeletePopup->getResult();
            mConfirmDeletePopup.reset();
            if (result && mAssetData) {
                ParticleSystemRepository::get().deleteAsset(mAssetData->getID());
                mAssetData = nullptr;
                mSelectedEmitter = nullptr;
            }
        }
    }
    else if (mDuplicateObjectPopup) {
        mDuplicateObjectPopup->setNames(getGlobalEmitterNames());
        if (mDuplicateObjectPopup->updateAndRender()) {
            duplicateGlobalEmitter(mDuplicateObjectPopup->getResultName());
            mDuplicateObjectPopup.reset();
        }
    } else if (ImGuiFileDialog::Instance()->Display(SAVE_DIALOG_NAME, ImGuiWindowFlags_NoCollapse, ImVec2(200.0f, 150.0f))) {
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
            ParticleSystemRepository::get().changeAssetFilePath(mAssetData->getID(), vio::Path(filePathName));
            if (!ParticleSystemRepository::get().saveAsset(mAssetData->getID())) {
                panic("FAILED TO SAVE PARTICLE SYSTEM {}", filePathName);
            }
        }

        ImGuiFileDialog::Instance()->Close();
    }
}

void ParticleSystemEditorViewportPanel::openDuplicateEmitterPopup() {
    mDuplicateObjectPopup = std::make_unique<ImguiUtil::CustomSelectorPopup>(getGlobalEmitterNames());
}

void ParticleSystemEditorViewportPanel::duplicateGlobalEmitter(const nString& emitterName) {
    if (emitterName.empty()) return;
    if (mAssetData == nullptr) return;

    nString name; // Share memory
    ParticleSystemRepository::get().forEachLoadedAsset([&](IAssetRepository<ParticleSystemDef>& repo, ParticleSystemDef& def) {
        name = def.getName().toString();
        for (auto& emitter : def.mEmitters) {
            if (emitterName == name + "." + emitter.mEmitterName.toString()) {
                mAssetData->mEmitters.emplace_back(emitter);
                mSelectedEmitter = &mAssetData->mEmitters.back();
                createPreviewSystem();
                return true;
            }
        }
        return false;
    });
}

std::vector<nString> ParticleSystemEditorViewportPanel::getGlobalEmitterNames() {
    std::vector<nString> emitterNames;
    nString name; // Share memory
    ParticleSystemRepository::get().forEachRegisteredAsset([&](ParticleSystemDef* def, const AssetMetadata& entry) {
        if (def) {
            name = def->getName().toString();
            for (auto& emitter : def->mEmitters) {
                emitterNames.push_back(name + "." + emitter.mEmitterName.toString());
            }
        }
        else {
            // Request asset load if needed
            ParticleSystemRepository& repo = ParticleSystemRepository::get();
            if (!mAssetHandleBundle.hasAssetHandle(entry.getId(), repo.getAssetType())) {
                mAssetHandleBundle.addAssetHandle(repo.getAssetHandle(entry.getId()));
            }
        }
        return false;
    });
    return emitterNames;
}
