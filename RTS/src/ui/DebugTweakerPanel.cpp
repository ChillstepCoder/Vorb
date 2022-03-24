#include "stdafx.h"
#include "DebugTweakerPanel.h"

#include "generation/WorldGeneration.h"
#include "editor/ImguiViews.hpp"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include "options/DebugOptions.h"

#include "definitions/ModelDef.h"
#include "ecs/EntityComponentSystem.h"

DebugTweakerPanel::DebugTweakerPanel(const f32v2& screenDims) : mScreenDims(screenDims)
{
}

void DebugTweakerPanel::updateAndRender(EntityComponentSystem& ecs, const vg::GBuffer* activeGBuffer, float aspectRatio)
{
    constexpr float WINDOW_WIDTH = 400.0f;
    const float WINDOW_HEIGHT = mScreenDims.y;
    ImGui::SetNextWindowPos(ImVec2(mScreenDims.x - WINDOW_WIDTH, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, WINDOW_HEIGHT));
    const ImVec2 buttonSize(WINDOW_WIDTH, 25);

    ImGui::Begin("Value Tweaker", &sDebugOptions.mShowTweaker, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
    ui32 ID = 10;

    if (ImGui::CollapsingHeader("Game Settings")) {
        ImGui::Checkbox("VSYNC", &sDebugOptions.mVSYNC);
        if (ImGui::SliderFloat("Load range", &sDebugOptions.mLoadRange, 128.0f, 3000.0f, "%.1f")) {
            sDebugOptions.mLoadRangeSq = SQ(sDebugOptions.mLoadRange);
        }
    }

    if (ImGui::CollapsingHeader("Grass")) {
        ImGui::PushID(++ID);
        if (ImGui::SliderFloat("Render distance", &sDebugOptions.mGrassSettings.distance, 100.0f, 500.0f, "%.1f")) {
            sDebugOptions.mGrassSettings.distanceSq = SQ(sDebugOptions.mGrassSettings.distance);
            sDebugOptions.mGrassSettings.fadeDistance = sDebugOptions.mGrassSettings.distance * GRASS_FADE_MULT;
        }
        ImGui::SliderFloat("Min LOD distance", &sDebugOptions.mGrassSettings.lodDistanceOffset, -50.0f, 150.0f, "%.1f");
        ImGui::Checkbox("Show LOD", &sDebugOptions.mDebugGrassLod);
        ImGui::Checkbox("Disable", &sDebugOptions.mHideGrass);
        ImGui::PopID();
    }

    if (ImGui::CollapsingHeader("Terrain")) {
        ImGui::PushID(++ID);
        ImGui::SliderFloat("Min LOD distance", &sDebugOptions.mTerrainLodDistanceOffset, 0.0f, 2500.0f, "%.1f");
        ImGui::Checkbox("Show LOD", &sDebugOptions.mDebugTerrainLod);
        ImGui::NewLine();
        ImGui::BeginChild("Terrain Funcs", ImVec2(WINDOW_WIDTH, 350.0f));
        sWorldGen.mIsDirty |= ImguiView::Noise::view(sWorldGen.mBaseNoise, ID);
        sWorldGen.mIsDirty |= ImguiView::Noise::view(sWorldGen.mMountainsNoise, ID);
        sWorldGen.mIsDirty |= ImguiView::Noise::view(sWorldGen.mMountainsDistNoise, ID);
        sWorldGen.mIsDirty |= ImguiView::Noise::view(sWorldGen.mContinentOutlineNoise, ID);
        sWorldGen.mIsDirty |= ImguiView::Noise::view(sWorldGen.mHumidityNoise, ID);
        sWorldGen.mIsDirty |= ImguiView::Noise::view(sWorldGen.mTemperatureNoise, ID);
        sWorldGen.mIsDirty |= ImguiView::Noise::view(sWorldGen.mForestNoise, ID);
        ImGui::EndChild();
        ImGui::NewLine();
        ImGui::PopID();
    }

    if (ImGui::CollapsingHeader("Camera Settings")) {
        ImGui::PushID(++ID);
        ImGui::SliderFloat("FoV", &sDebugOptions.mFoV, 1.0f, 179.0f, "%.1f");
        ImGui::SliderFloat("Far Plane", &sDebugOptions.mZFar, 10000.0f, 300000.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Z Height", &sDebugOptions.mCameraZHeight, 0.3f, 10.0f, "%.1f");
        ImGui::SliderFloat("XY Distance", &sDebugOptions.mCameraXYDistance, 0.3f, 10.0f, "%.1f");
        ImGui::Text("Camera mode");
        if (ImGui::RadioButton("Free look", sDebugOptions.mCameraMode == CameraMode::FREE_LOOK)) {
            sDebugOptions.mCameraMode = CameraMode::FREE_LOOK;
        }
        if (ImGui::RadioButton("Mouselock basic", sDebugOptions.mCameraMode == CameraMode::MOUSELOCK_BASIC)) {
            sDebugOptions.mCameraMode = CameraMode::MOUSELOCK_BASIC;
        }
        if (ImGui::RadioButton("Cartesian", sDebugOptions.mCameraMode == CameraMode::CARTESIAN)) {
            sDebugOptions.mCameraMode = CameraMode::CARTESIAN;
        }
        static_assert(e_cast(CameraMode::COUNT) == 4, "Update options");
        ImGui::PopID();
    }
    
    if (ImGui::CollapsingHeader("Clouds")) {
        ImGui::PushID(++ID);
        ImGui::Checkbox("Disable", &sDebugOptions.mDisableClouds);
        ImGui::SliderInt("Blur Passes", &sDebugOptions.mCloudBlurPasses, 0, 15);
        ImGui::SliderFloat("Blur Radius", &sDebugOptions.mCloudBlurRadius, 0.0f, 15.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Ambient", &sDebugOptions.mCloudAmbient, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("Speed", &sDebugOptions.mCloudSpeed, 0.0f, 50.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::PopID();
    }

    if (ImGui::CollapsingHeader("Depth of Field")) {
        ImGui::PushID(++ID);
        ImGui::SliderInt("Blur Passes", &sDebugOptions.mDepthOfFieldBlurPasses, 0, 15);
        ImGui::SliderFloat("Blur Radius", &sDebugOptions.mDepthOfFieldBlurRadius, 0.0f, 15.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::PopID();
    }

    if (ImGui::CollapsingHeader("Ambient Occlusion")) {
        ImGui::PushID(++ID);
        ImGui::Checkbox("Disable", &sDebugOptions.mSSAODisabled);
        ImGui::SliderInt("Blur Passes", &sDebugOptions.mSSAOBlurPasses, 0, 15);
        ImGui::SliderFloat("Blur Radius", &sDebugOptions.mSSAOBlurRadius, 0.0f, 15.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Bias", &sDebugOptions.mSSAOBias, 0.0f, 0.2f);
        ImGui::SliderFloat("Radius", &sDebugOptions.mSSAORadius, 0.001f, 4.0f);
        ImGui::SliderFloat("Range Check Mult", &sDebugOptions.mSSAORangeCheckMult, 0.01f, 1.5f);
        ImGui::ColorPicker3("Color", &sDebugOptions.mSSAOColor.x, ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar);
        ImGui::PopID();
    }

    if (ImGui::CollapsingHeader("Shadows")) {
        ImGui::PushID(++ID);
        ImGui::Checkbox("Disable", &sDebugOptions.mDisableShadows);
        ImGui::SliderFloat("Z Mult", &sDebugOptions.mShadowZMult, 0.0f, 100.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Update Rate Seconds", &sDebugOptions.mShadowUpdateRateSeconds, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Near Cascade Size", &sDebugOptions.mShadowNearSize, 10.0f, 300.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderInt("Blur Passes", &sDebugOptions.mShadowBlurPasses, 0, 15);
        ImGui::SliderFloat("Blur Radius", &sDebugOptions.mShadowBlurRadius, 0.0f, 15.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::ColorPicker3("Color", &sDebugOptions.mShadowColor.x, ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar);
        ImGui::PopID();
    }

    if (ImGui::CollapsingHeader("Toggles")) {
        ImGui::Checkbox("Pause Frustum Updates", &sDebugOptions.mPauseFrustum);
        ImGui::Checkbox("Show Wireframe", &sDebugOptions.mWireframe);
        ImGui::Checkbox("Show Chunk Boundaries", &sDebugOptions.mChunkBoundaries);
        ImGui::Checkbox("Show City Debug", &sDebugOptions.mCities);
        ImGui::Checkbox("Show Roof Debug", &sDebugOptions.mRoofDebug);
        ImGui::Checkbox("Show Navgraph", &sDebugOptions.mShowNavGraph);
        ImGui::Checkbox("Show Navgraph Updates", &sDebugOptions.mShowNavGraphUpdates);
        ImGui::Checkbox("Show Physics Debug", &sDebugOptions.mShowPhysicsDebug);
        ImGui::Checkbox("Show Business Debug", &sDebugOptions.mShowBusinessDebug);
        ImGui::Checkbox("Show Paths", &sDebugOptions.mShowPaths);
        ImGui::Checkbox("Show Entity Queries", &sDebugOptions.mShowEntityQueries);
        ImGui::Checkbox("Hide Characters", &sDebugOptions.mHideCharacters);
    }
    if (activeGBuffer) {
        if (ImGui::CollapsingHeader("GBuffer")) {
            const ImVec2 uv0(0, 1);
            const ImVec2 uv1(1, 0);
            const ImVec2 dims(WINDOW_WIDTH, WINDOW_WIDTH / aspectRatio);
            ImGui::Text("Geometry");
            ImGui::Image((ImTextureID)activeGBuffer->getGeometryTexture(), dims, uv0, uv1);
            ImGui::Text("Normals");
            ImGui::Image((ImTextureID)activeGBuffer->getNormalTexture(), dims, uv0, uv1);
            ImGui::Text("Roughness");
            ImGui::Image((ImTextureID)activeGBuffer->getRoughnessTexture(), dims, uv0, uv1);
            ImGui::Text("Depth");
            ImGui::Image((ImTextureID)activeGBuffer->getDepthTexture(), dims, uv0, uv1);
        }
    }

    if (ImGui::CollapsingHeader("Shader Tweaker")) {
        ImGui::PushID(++ID);
        ImGui::ColorPicker3("Debug Color 1", &sDebugOptions.mDebugColor01.x, ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar);
        ImGui::ColorPicker3("Debug Color 2", &sDebugOptions.mDebugColor02.x, ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar);
        ImGui::SliderFloat("Debug Float 1", &sDebugOptions.mDebugFloat01, 0.0f, 1.0f);
        ImGui::SliderFloat("Debug Float 2", &sDebugOptions.mDebugFloat02, 0.0f, 1.0f);
        ImGui::PopID();
    }

    if (ImGui::CollapsingHeader("Animation Debugger")) {
        ImGui::PushID(++ID);
        
        CharacterModelComponent& playerModel = ecs.mRegistry.get<CharacterModelComponent>(ecs.mPlayerEntity);
        ui32 numActive = 0;
        for (int i = 0; i < NUM_ANIM_TRACKS; ++i) {
            AnimTrack& track = playerModel.mAnimState.mTracks[i]; // I'm basically God
            const ozz::animation::Animation* anim = playerModel.mModel->mAnimMachine->mAnimsArray[i];
            if (anim) {
                bool isActive = track.mFlags.isBitSet(AnimTrackFlags::IS_ACTIVE);
                if (ImGui::Checkbox((nString("Is Active ") + std::to_string(i)).c_str(), &isActive)) {
                    if (isActive) {
                        track.mFlags.setBit(AnimTrackFlags::IS_ACTIVE);
                    }
                    else {
                        track.mFlags.clearBit(AnimTrackFlags::IS_ACTIVE);
                    }
                }
                if (track.isActive()) {
                    ++numActive;
                }
                if (ImGui::SliderFloat(AnimMachineStateNames[i], &track.mWeightScale, 0.0f, 1.0f)) {
                    // Debug update the context
                    playerModel.setAnimTrackWeight(AnimMachineState(i), track.mWeight);
                }
                ImGui::SliderFloat((nString("Time ") + std::to_string(i)).c_str(), &track.mTime, 0.0f, track.mDuration);
                f32 fadeWeight = (f32)track.mWeight / MAX_ANIM_FADE_WEIGHT;
                ImGui::SliderFloat((nString("Weight ") + std::to_string(i)).c_str(), &fadeWeight, 0.0f, 1.0f);
                ImGui::Separator();
            }
        }
        ImGui::Separator();
        ImGui::Text((nString("Total Active Anims: ") + std::to_string(numActive)).c_str());
        ImGui::SliderFloat((nString("Footstep alpha ")).c_str(), &playerModel.mFootstepAlpha, 0.0f, 1.0f);
        ImGui::PopID();
    }

    glGetString(GL_VENDOR);
    if (ImGui::CollapsingHeader("GPU Stats")) {
        const char* vendor = (const char*)glGetString(GL_VENDOR);
        const char* renderer = (const char*)glGetString(GL_RENDERER);
        ImGui::Text((nString("Vendor: ") + nString(vendor)).c_str());
        ImGui::Text((nString("Renderer: ") + nString(renderer)).c_str());

        //https://www.geeks3d.com/20100531/programming-tips-how-to-know-the-graphics-memory-size-and-usage-in-opengl/
        if (strcmp(vendor, "NVIDIA Corporation") == 0) {
#define GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX 0x9048
#define GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX 0x9049
            GLint total_mem_kb = 0;
            GLint cur_avail_mem_kb = 0;
            glGetIntegerv(GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX, &total_mem_kb);
            glGetIntegerv(GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX, &cur_avail_mem_kb);
            GLint totalUsedMem = total_mem_kb - cur_avail_mem_kb;

            ImGui::Text((nString("Total VRAM: ") + std::to_string(total_mem_kb / 1000) + " mb").c_str());
            ImGui::Text((nString("Available VRAM: ") + std::to_string(cur_avail_mem_kb / 1000) + " mb").c_str());
            ImGui::Text((nString("Used VRAM: ") + std::to_string(totalUsedMem / 1000) + " mb").c_str());
        }
    }

    // Uncomment to learn imgui
    //bool show = true;
    //ImGui::ShowDemoWindow(&show);

    ImGui::End();
}
