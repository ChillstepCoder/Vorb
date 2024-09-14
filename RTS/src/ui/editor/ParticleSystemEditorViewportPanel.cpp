#include "stdafx.h"
#include "ParticleSystemEditorViewportPanel.h"

#include "rendering/RenderContext.h"
#include "resources/MaterialRepository.h"
#include "resources/ParticleSystemRepository.h"
#include "resources/ModelRepository.h"

#include "rendering/particle/CPUParticleSystem.h"
#include "rendering/MaterialShaderRepository.h"

#include "camera/SimpleCamera.h"
#include "camera/Camera3D.h"

#include "ui/UIContext.h"
#include "ui/ImguiUtil.hpp"
#include "ui/editor/ImguiAssetThumbnails.h"

#include <Vorb/io/FileOps.h>

#include "definitions/ModelDef.h"

#include <imgui.h>

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

enum class PartcileSystemPopupAssetType {
    ParticleSystem,
    ParticleEmitter
};

ParticleSystemEditorViewportPanel::ParticleSystemEditorViewportPanel() {

}

ParticleSystemEditorViewportPanel::~ParticleSystemEditorViewportPanel() {

}

void ParticleSystemEditorViewportPanel::updateAndRenderInternal(f32 elapsedSec) {
    mCurrentTime += elapsedSec;

    if (mAssetWasChanged) {
        mAssetWasChanged = false;
        if (mAssetData) {
            unselect();
            mSelectedEmitter = &mAssetData->mEmitters[0];
        }
        createPreviewSystem();
    }

    if (!mAssetData) {
        mCurrentTime = mTimelineEnd;
        unselect();
    }
    if (mCurrentTime >= mTimelineEnd) {
        createPreviewSystem();
    }
}

void ParticleSystemEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize) {

    // Add FPS for convenience
    char buffer[64];
    sprintf_s(buffer, sizeof(buffer), "(%.3f ms)", mUpdateTimeMs);
    ImGui::Text(buffer);

    if (mPreviewSystem) {
        if (mShowEmitters.size() == mPreviewSystem->getNumEmitters()) {
            mNumParticles = mPreviewSystem->getNumParticles();
        }

        if (!mSelectedEmitter && mAssetData->mEmitters.size()) {
            unselect();
            mSelectedEmitter = &mAssetData->mEmitters[0];
        }
    }
    else {
        mNumParticles = 0;
    }
    sprintf_s(buffer, sizeof(buffer), "Particles: %d", (int)mNumParticles);
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
    std::tuple<std::string, void*, int> renamePopupData;

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
                defaultEmitter.mMaterialRef = ParticleSystemRepository::get().getDefaultMaterialID();
                defaultEmitter.mShaderRef = CStrToken("particle_3d");

                unselect();
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

        ImGui::Separator();
        ImGui::Text("System: %s", mAssetData->getName().toString().c_str());
        if (ImGui::Button("Rename")) {
            renamePopupData = std::make_tuple(
                mAssetData->getName().toString(),
                (void*)mAssetData,
                (int)PartcileSystemPopupAssetType::ParticleSystem
            );
        }
        ImGui::SliderFloat("Lifetime", &mAssetData->mLifetimeSec, 0.0f, 60.0f);

        updateAndRenderSaveButton();
        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
            mConfirmDeletePopup = std::make_unique<ImguiUtil::ConfirmDeletePopup>(mAssetData->getName().toString(), (void*)mAssetData, (int)PartcileSystemPopupAssetType::ParticleSystem);
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
                newEmitterDef.mMaterialRef = ParticleSystemRepository::get().getDefaultMaterialID();
                newEmitterDef.mShaderRef = CStrToken("particle_3d");

                unselect();
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
                    unselect();
                    mSelectedEmitter = &emitter;
                }
            }

            if (ImGui::BeginPopupContextItem(emitter.mEmitterName.toString().c_str())) {
                if (ImGui::MenuItem("Delete")) {
                    if (mSelectedEmitter == &emitter) {
                        unselect();
                    }
                    mAssetData->mEmitters.erase(mAssetData->mEmitters.begin() + i);
                }
                if (ImGui::MenuItem("Rename"))
                {
                    renamePopupData = std::make_tuple(
                        emitter.mEmitterName.toString(),
                        (void*)&emitter,
                        (int)PartcileSystemPopupAssetType::ParticleEmitter
                    );
                }
                ImGui::EndPopup();
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

        if (std::get<0>(renamePopupData).size()) {
            mRenamePopup = std::make_unique<ImguiUtil::RenameAssetPopup>(
                std::get<0>(renamePopupData),
                std::get<1>(renamePopupData),
                std::get<2>(renamePopupData)
            );
        }

        ImGui::Spacing();
        ImGui::Separator();

        if (mAssetData) {
            bool changed = false;

            if (ImGui::CollapsingHeader("Inputs", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Button("+")) {
                    mInputSelectorPopup = std::make_unique<ImguiUtil::EnumSelectorPopup<ParticleSystemInputName>>("InputSelector", "Select Input", [&](ParticleSystemInputName v) {
                        return v != ParticleSystemInputName::INVALID && !mAssetData->mDefaultInputs->inputMap.contains(v);
                    });
                }
                for (auto it = mAssetData->mDefaultInputs->inputMap.begin(); it != mAssetData->mDefaultInputs->inputMap.end();) {
                    ImGui::PushID(e_cast(it->first));
                    ImGui::Text(ENUM_CSTR(ParticleSystemInputName, it->first));
                    ImGui::SameLine();
                    if (uint* v = std::get_if<uint>(&it->second)) {
                        int iv = *v;
                        if (ImGui::InputInt("##input", &iv)) {
                            changed = true;
                        }
                        *v = iv;
                    }
                    else if (f32* v = std::get_if<f32>(&it->second)) {
                        if (ImGui::InputFloat("##input", v)) {
                            changed = true;
                        }
                    }
                    else if (f32v2* v = std::get_if<f32v2>(&it->second)) {
                        if (ImGui::InputFloat2("##input", &v->x)) {
                            changed = true;
                        }
                    }
                    else if (f32v3* v = std::get_if<f32v3>(&it->second)) {
                        if (ImGui::InputFloat3("##input", &v->x)) {
                            changed = true;
                        }
                    }
                    else if (ParticleSystemMeshInput* v = std::get_if<ParticleSystemMeshInput>(&it->second)) {
                        ModelAssetRef modelRef = v->mModelDef ? ModelAssetRef(v->mModelDef->getID()) : ModelAssetRef();
                        if (ImguiUtil::updateAndRenderAssetReference("Model", modelRef, (ui64)it->first, AssetType::Model)) {
                            changed = true;
                            v->mModelDef = modelRef.isValid() ? &ModelRepository::get().getLoadedOrUnloadedAsset(modelRef.getAssetID()) : nullptr;
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("X")) {
                        it = mAssetData->mDefaultInputs->inputMap.erase(it);
                        changed = true;
                    }
                    else {
                        ++it;
                    }
                    ImGui::PopID();
                }
            }

            ImGui::Spacing();

            if (ImGui::CollapsingHeader("User Params", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Button("+")) {

                }
            }

            if (changed) {
                ParticleSystemRepository::get().onAssetChangedByEditor(mAssetData->getID());
                createPreviewSystem();
            }
        }

        // Popups are opened in this panel
        updatePopups();
    }
}

bool ParticleSystemEditorViewportPanel::updateAndRenderSecondaryControls(f32 ySize) {
    ImGui::BeginChild("Particle Emitter Editor", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings/* | ImGuiWindowFlags_NoScrollbar*/);

    if (!mAssetData) {
        ImGui::Text("Select a Particle System");
        ImGui::EndChild();
        return true;
    }

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

    bool changed = false;

    if (mSelectedEmitter) {
        ImGui::Text("Emitter: %s", mSelectedEmitter->mEmitterName.toString().c_str());
        ImGui::Separator();
        ImGui::Text("Modules");
        const ImVec2 contentAvail = ImGui::GetContentRegionAvail();
        constexpr int size = 17;
        const int moduleButtonWidth = contentAvail.x - size * 3;

        auto displayCategory = [&](const char* popupName, const char* categoryName, ParticleEmitterModuleStage stage, CPUParticleEmitterModuleVector& modules, f32 r, f32 g, f32 b) -> bool {
            bool changed = false;
            ImGui::PushID(categoryName);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(ImVec4(r, g, b, 0.8f))); 
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor(ImVec4(r, g, b, 0.9f))); 
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor(ImVec4(r, g, b, 1.0f))); 
            ImGui::Button(categoryName, ImVec2(moduleButtonWidth, size));
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
            if (ImGui::Button("+", ImVec2(size, size))) {
                ImGui::OpenPopup(popupName);
            }

            int mid = 0;
            int moduleIndex = 0;
            for (auto&& it = modules.begin(); it != modules.end();) {
                bool didDelete = false;
                ImGui::PushID(++mid);
                auto& module = *it;
                if (mSelectedModule == module.get()) {
                    SELECTED_BUTTON(ImGui::Button(module->getName(), ImVec2(moduleButtonWidth, size)));
                }
                else {
                    if (module->isValid()) {
                        if (ImGui::Button(module->getName(), ImVec2(moduleButtonWidth, size))) {
                            mSelectedModule = module.get();
                            mSelectedModuleVector = &modules;
                        }
                    }
                    else {
                        ImguiUtil::ScopedColor color(ImGuiCol_Button, ImguiColors::Theme::error);
                        if (ImGui::Button(module->getName(), ImVec2(moduleButtonWidth, size))) {
                            mSelectedModule = module.get();
                            mSelectedModuleVector = &modules;
                        }
                        ImGui::SetItemTooltip("Module is invalid due to missing prerequisites");
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("-", ImVec2(size, size))) {
                    if (mSelectedModule == module.get()) {
                        mSelectedModule = nullptr;
                        mSelectedModuleVector = nullptr;
                    }
                    it = modules.erase(it);
                    changed = true;
                    didDelete = true;
                }
                ImGui::SameLine();
                // Up and down arrows
                if (moduleIndex == modules.size() - 1) {
                    ImGui::InvisibleButton(" ", ImVec2(size, size));
                }
                else if (ImGui::Button("v", ImVec2(size, size))) {
                    changed = true;
                    std::swap(modules[moduleIndex + 1], modules[moduleIndex]);
                }
                ImGui::SameLine();
                if (moduleIndex == 0) {
                    ImGui::InvisibleButton(" ", ImVec2(size, size));
                }
                else if (ImGui::Button("^", ImVec2(size, size))) {
                    changed = true;
                    std::swap(modules[moduleIndex - 1], modules[moduleIndex]);
                }
                ImGui::PopID();

                if (!didDelete) {
                    ++it;
                    ++moduleIndex;
                }
            }

            ImGui::PopStyleVar(4);

            if (ImGui::BeginPopupModal(popupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                if (const CPUParticleEmitterModule* displayModule = displayModuleSelectorCombo(stage)) {
                    mSelectedModule = modules.emplace_back(displayModule->clone()).get();
                    mSelectedModuleVector = &modules;
                    changed = true;
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
            return changed;
        };

        changed |= displayCategory("Emitter Update Modules", "Emitter Update", ParticleEmitterModuleStage::EmitterUpdate, mSelectedEmitter->mModules.mEmitterUpdate, 0.0f, 0.8f, 0.0f);
        ImGui::Spacing();
        changed |= displayCategory("Particle Init Modules", "Particle Init", ParticleEmitterModuleStage::ParticleInit, mSelectedEmitter->mModules.mParticleInit, 0.7f, 0.7f, 0.0f);
        ImGui::Spacing();
        changed |= displayCategory("Particle Update Modules", "Particle Update", ParticleEmitterModuleStage::ParticleUpdate, mSelectedEmitter->mModules.mParticleUpdate, 0.8f, 0.0f, 0.0f);
    }
    else {
        ImGui::Text("Select a Particle Emitter");
    }

    ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
    ImGui::Spacing(); ImGui::Separator();

    if (mSelectedEmitter) {
        ImGui::Text("Emitter: %s", mSelectedEmitter->mEmitterName.toString().c_str());
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
        changed |= ImguiUtil::EnumCombo("Blend Mode", mSelectedEmitter->mBlendMode);
       
        if (ImguiUtil::updateAndRenderAssetReference(nullptr, mSelectedEmitter->mMaterialRef, AssetType::Material)) {
            changed = true;
        }

        if (ImguiUtil::updateAndRenderAssetReference(nullptr, mSelectedEmitter->mShaderRef, AssetType::MaterialShader, [](AssetID id) {
            vio::Path path = MaterialShaderRepository::get().getAssetFilePath(id);
            return vio::containsSubpath(path, "particle");
        })) {
            changed = true;
        }
        ImGui::SeparatorText("Required Shader Inputs");
        for (const ParticleEmitterShaderBinding& binding : mSelectedEmitter->mShaderBindings) {
            std::visit([&](auto arg) {
                bool hasVariable = false;
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, ParticleEmitterVariableNameUInt>) {
                    hasVariable = std::find(mSelectedEmitter->mUIntVariables.begin(), mSelectedEmitter->mUIntVariables.end(), arg) != mSelectedEmitter->mUIntVariables.end();
                }
                else if constexpr (std::is_same_v<T, ParticleEmitterVariableNameFloat>) {
                    hasVariable = std::find(mSelectedEmitter->mFloatVariables.begin(), mSelectedEmitter->mFloatVariables.end(), arg) != mSelectedEmitter->mFloatVariables.end();
                }
                else if constexpr (std::is_same_v<T, ParticleEmitterVariableNameVec2>) {
                    hasVariable = std::find(mSelectedEmitter->mVec2Variables.begin(), mSelectedEmitter->mVec2Variables.end(), arg) != mSelectedEmitter->mVec2Variables.end();
                }
                else if constexpr (std::is_same_v<T, ParticleEmitterVariableNameVec3>) {
                    hasVariable = std::find(mSelectedEmitter->mVec3Variables.begin(), mSelectedEmitter->mVec3Variables.end(), arg) != mSelectedEmitter->mVec3Variables.end();
                }
                static_assert(TOTAL_PARTICLE_EMITTER_VARIABLE_TYPES == 4);

                if (hasVariable) {
                    ImGui::Text("%u - %s", binding.mShaderBindingIndex, ENUM_CSTR(T, arg));
                }
                else {
                    ImguiUtil::ScopedColor color(ImGuiCol_Text, ImguiColors::Theme::textError);
                    ImGui::Text("%u - %s (MISSING)", binding.mShaderBindingIndex, ENUM_CSTR(T, arg));
                }
            }, binding.mVariableName);
        }

    }
    if (changed) {
        ParticleSystemRepository::get().onAssetChangedByEditor(mAssetData->getID());
        createPreviewSystem();
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
        ImGui::Separator();

        assert(mSelectedEmitter);
        if (mSelectedModule->updateAndRenderEditorControls(*mSelectedEmitter)) {
            ParticleSystemRepository::get().onAssetChangedByEditor(mAssetData->getID());
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
    renderGrid(mCamera->getViewProjectionMatrix());

    // Render preview system
    if (mPreviewSystem) {
        Camera3D camera;
        camera.copyFromSimpleCamera(*mCamera);
        RenderContext::getInstance().updateGlobalUbo(f32v3(0.0f), camera),
        MaterialRepository::get().bindMaterialBuffer();

        PreciseTimer timer;
        mPreviewSystem->updateAndRenderEditor(mCurrentElapsedSec, mCamera->getViewProjectionMatrix(), mShowEmitters);
        mUpdateTimeMs = timer.elapsedMs();
    }
    else {
        mUpdateTimeMs = 0.0f;
    }
}

void ParticleSystemEditorViewportPanel::createPreviewSystem() {
    if (!mAssetData) return;
    mCurrentTime = 0.0f;
    mPreviewSystem = std::make_unique<CPUParticleSystem>(*mAssetData, f32v3(0.0f), mAssetData->mDefaultInputs);
    mPreviewSystem->setAsEditorPreviewSystem();
}

void ParticleSystemEditorViewportPanel::updatePopups() {
    if (mRenamePopup) {
        if (mRenamePopup->updateAndRender()) {
            const nString& result = mRenamePopup->getResult();
            if (result.size()) {
                if (mRenamePopup->getAssetType() == (int)PartcileSystemPopupAssetType::ParticleSystem) {
                    ParticleSystemRepository::get().renameAsset(AssetDescriptor::fromIAsset(*((ParticleSystemDef*)mRenamePopup->getAssetPtr())), result);
                } else if (mRenamePopup->getAssetType() == (int)PartcileSystemPopupAssetType::ParticleEmitter) {
                    ParticleEmitterDef* emitterDef = (ParticleEmitterDef*)mRenamePopup->getAssetPtr();
                    emitterDef->mEmitterName = StrToken(result);
                }
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
                unselect();
            }
        }
    }
    else if (mDuplicateObjectPopup) {
        mDuplicateObjectPopup->setNames(getGlobalEmitterNames());
        if (mDuplicateObjectPopup->updateAndRender()) {
            duplicateGlobalEmitter(mDuplicateObjectPopup->getResultName());
            mDuplicateObjectPopup.reset();
        }
    }
    else if (mInputSelectorPopup) {
        if (mInputSelectorPopup->updateAndRender()) {
            const ParticleSystemInputName input = mInputSelectorPopup->getResult();
            if (input != ParticleSystemInputName::COUNT) {
                mAssetData->mDefaultInputs->addDefaultInput(input);
                ParticleSystemRepository::get().onAssetChangedByEditor(mAssetData->getID());
                createPreviewSystem();
            }
            mInputSelectorPopup.reset();
        }
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
                unselect();
                mSelectedEmitter = &mAssetData->mEmitters.back();
                ParticleSystemRepository::get().onAssetChangedByEditor(mAssetData->getID());
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

void ParticleSystemEditorViewportPanel::unselect() {
    mSelectedEmitter = nullptr;
    mSelectedModule = nullptr;
    mSelectedModuleVector = nullptr;
}
