#include "stdafx.h"
#include "DebugTweakerPanel.h"

#include "generation/WorldGeneration.h"
#include "editor/ImguiViews.hpp"

#include "debugging/VisualLogger.h"

#include "world/IWorld.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include "rendering/GLExtensions.h"

#include "options/DebugOptions.h"

#include "definitions/ModelDef.h"
#include "ecs/IEntityComponentSystem.h"

#include "debugging/ValueTweaker.h"

#include <Vorb/graphics/GBuffer.h>

// TODO: Use
void setDefaultTheme() {
    // Colors
    ImVec4 blackSemi = ImVec4(0.00f, 0.00f, 0.00f, 0.94f);
    ImVec4 black = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    ImVec4 red = ImVec4(0.23f, 0.16f, 0.16f, 1.00f);
    ImVec4 beige = ImVec4(0.78f, 0.62f, 0.51f, 1.00f);
    ImVec4 darkGrey = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    ImVec4 greenGrey = ImVec4(0.148f, 0.168f, 0.153f, 1.00f);
    ImVec4 white = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    ImVec4 darkerGrey = ImVec4(0.03f, 0.03f, 0.03f, 1.00f);
    ImVec4 grey = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    ImVec4 green = ImVec4(0.21f, 0.27f, 0.27f, 1.00f);

    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text] = beige;
    colors[ImGuiCol_TextDisabled] = white;
    colors[ImGuiCol_WindowBg] = black;
    colors[ImGuiCol_ChildBg] = black;
    colors[ImGuiCol_PopupBg] = black;
    colors[ImGuiCol_Border] = beige;
    colors[ImGuiCol_BorderShadow] = black;
    colors[ImGuiCol_FrameBg] = red;
    colors[ImGuiCol_FrameBgHovered] = darkerGrey;
    colors[ImGuiCol_FrameBgActive] = black;
    colors[ImGuiCol_TitleBg] = black;
    colors[ImGuiCol_TitleBgActive] = black;
    colors[ImGuiCol_TitleBgCollapsed] = black;
    colors[ImGuiCol_MenuBarBg] = red;
    colors[ImGuiCol_ScrollbarBg] = black;
    colors[ImGuiCol_ScrollbarGrab] = red;
    colors[ImGuiCol_ScrollbarGrabHovered] = darkerGrey;
    colors[ImGuiCol_ScrollbarGrabActive] = grey;
    colors[ImGuiCol_CheckMark] = beige;
    colors[ImGuiCol_SliderGrab] = beige;
    colors[ImGuiCol_SliderGrabActive] = beige;
    colors[ImGuiCol_Button] = red;
    colors[ImGuiCol_ButtonHovered] = darkerGrey;
    colors[ImGuiCol_ButtonActive] = red;
    colors[ImGuiCol_Header] = red;
    colors[ImGuiCol_HeaderHovered] = darkerGrey;
    colors[ImGuiCol_HeaderActive] = black;
    colors[ImGuiCol_Separator] = beige;
    colors[ImGuiCol_SeparatorHovered] = darkerGrey;
    colors[ImGuiCol_SeparatorActive] = beige;
    colors[ImGuiCol_ResizeGrip] = black;
    colors[ImGuiCol_ResizeGripHovered] = darkerGrey;
    colors[ImGuiCol_ResizeGripActive] = black;
    colors[ImGuiCol_Tab] = red;
    colors[ImGuiCol_TabHovered] = darkerGrey;
    colors[ImGuiCol_TabActive] = darkerGrey;
    colors[ImGuiCol_TabUnfocused] = black;
    colors[ImGuiCol_TabUnfocusedActive] = black;
    colors[ImGuiCol_PlotLines] = beige;
    colors[ImGuiCol_PlotLinesHovered] = darkerGrey;
    colors[ImGuiCol_PlotHistogram] = beige;
    colors[ImGuiCol_PlotHistogramHovered] = darkerGrey;
    colors[ImGuiCol_TextSelectedBg] = black;
    colors[ImGuiCol_DragDropTarget] = beige;
    colors[ImGuiCol_NavHighlight] = black;
    colors[ImGuiCol_NavWindowingHighlight] = black;
    colors[ImGuiCol_NavWindowingDimBg] = black;
    colors[ImGuiCol_ModalWindowDimBg] = black;

    // IO
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 1.6f;
}

void renderLightingUI(ui32& ID, LightingOptions* options, int presetIndex) {
    ImGui::PushID(++ID);
    ImGui::SliderFloat("Gamma", &options->mGamma, 0.0f, 4.0f);
    ImGui::SliderFloat("Exposure", &options->mExposure, 0.0f, 4.0f);
    ImGui::SliderFloat("Haze Exponent", &options->mHazeExponent, 0.0f, 2.0f);
    ImGui::SliderFloat("Haze Divisor", &options->mHazeDivisor, 10.0f, 15000.0f);
    ImGui::SliderFloat("Ambient Light", &options->mAmbient, 0.0f, 1.0f);
    ImGui::SliderFloat("Sun Intensity", &options->mSunIntensity, 0.0f, 3.0f);
    switch (options->mToneMapOperator) {
        case 0:
            ImGui::Text("TONEMAP: NONE");
            break;
        case 1:
            ImGui::Text("TONEMAP: REINARD");
            break;
        case 2:
            ImGui::Text("TONEMAP: LOTTES");
            break;
        case 3:
            ImGui::Text("TONEMAP: UCHIMURA");
            break;
        case 4:
            ImGui::Text("TONEMAP: UNREAL");
            break;
        case 5:
            ImGui::Text("TONEMAP: FILMIC");
            break;
        case 6:
            ImGui::Text("TONEMAP: UNCHARTED 2");
            break;
    }
    ImGui::SliderInt("Tonemap Operator", &options->mToneMapOperator, 0, 6);

    switch (options->mLightingModel) {
        case 0:
            ImGui::Text("LIGHTMODEL: PHONG");
            break;
        case 1:
            ImGui::Text("LIGHTMODEL: BLINN_PHONG");
            break;
    }
    ImGui::SliderInt("Lighting model", &options->mLightingModel, 0, e_cast(LIGHTING_MODEL::COUNT) - 1);
    if (ImGui::Button("Reset to Default")) {
        *options = sLightingPresetDefaults[presetIndex];
    }
    ImGui::PopID();
}

// Use the manual it rocks
// https://pthom.github.io/imgui_manual_online/manual/imgui_manual.html
void DebugTweakerPanel::updateAndRender(const vg::GBuffer* activeGBuffer, float ySize, float aspectRatio)
{
    IEntityComponentSystem& ecs = sWorld->getECS();

    ImGui::BeginChild("Value Tweaker", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Text("Value Tweaker");
    ui32 ID = 10;

    if (ImGui::CollapsingHeader("Game Settings")) {
        ImGui::Checkbox("VSYNC", &sDebugOptions.mVSYNC);
        if (ImGui::SliderFloat("Load range", &sDebugOptions.mLoadRange, 128.0f, 3000.0f, "%.1f")) {
            sDebugOptions.mLoadRangeSq = SQ(sDebugOptions.mLoadRange);
        }
        ImGui::Separator();
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
        ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Terrain")) {
        ImGui::PushID(++ID);
        ImGui::Checkbox("Disable", &sDebugOptions.mDisableTerrain);
        ImGui::SliderFloat("Min LOD distance", &sDebugOptions.mTerrainLodDistanceOffset, 0.0f, 2500.0f, "%.1f");
        ImGui::Checkbox("Show LOD", &sDebugOptions.mDebugTerrainLod);
        ImGui::Separator();
        ImGui::Text("Color");
        ImGui::SliderFloat("Height Mult", &sDebugOptions.mTerrainHeightColorMult, 0.0f, 1.0f);
        ImGui::SliderFloat("Wavy Mult", &sDebugOptions.mTerrainWavyColorMult, 0.0f, 1.0f);
        ImGui::SliderFloat("Squares Period", &sDebugOptions.mTerrainSquaresColorPeriod, 0.0f, 1.0f);
        ImGui::SliderFloat("Squares Intensity", &sDebugOptions.mTerrainSquaresIntensity, 0.0f, 1.0f);
        ImGui::SliderFloat("Blend Mult", &sDebugOptions.mTerrainBlendMult, 0.0f, 1.0f);
        ImGui::Separator();
        ImGui::NewLine();
        ImGui::BeginChild("Terrain Funcs", ImVec2(0.0f, 0.0f));
        sWorldGen.mIsDirty |= ImguiView::Noise::view(sWorldGen.mBaseNoise, ID);
        sWorldGen.mIsDirty |= ImguiView::Noise::view(sWorldGen.mMountainsNoise, ID);
        sWorldGen.mIsDirty |= ImguiView::Noise::view(sWorldGen.mMountainsDistNoise, ID);
        sWorldGen.mIsDirty |= ImguiView::Noise::view(sWorldGen.mContinentOutlineNoise, ID);
        sWorldGen.mIsDirty |= ImguiView::Noise::view(sWorldGen.mHumidityNoise, ID);
        sWorldGen.mIsDirty |= ImguiView::Noise::view(sWorldGen.mTemperatureNoise, ID);
        sWorldGen.mIsDirty |= ImguiView::Noise::view(sWorldGen.mForestNoise, ID);
        ImGui::EndChild();
        ImGui::PopID();
        ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Water")) {
        ImGui::PushID(++ID);
        ImGui::Checkbox("Disable", &sDebugOptions.mDisableWater);
        ImGui::ColorPicker4("Shallow Color", &sDebugOptions.mShallowWaterColor.x, ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar);
        ImGui::ColorPicker4("Deep Color", &sDebugOptions.mDeepWaterColor.x, ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar);
        ImGui::ColorPicker4("Foam Color", &sDebugOptions.mWaterFoamColor.x, ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar);
        ImGui::SliderFloat("Distort Amount", &sDebugOptions.mWaterSurfaceDistortAmount, 0.0f, 1.0f);
        ImGui::SliderFloat("Move Speed", &sDebugOptions.mWaterSurfaceMoveSpeed, 0.0f, 1.0f);
        ImGui::SliderFloat("Noise Cutoff", &sDebugOptions.mWaterSurfaceNoiseCutoff, 0.0f, 1.0f);
        ImGui::SliderFloat("Smoothstep AA", &sDebugOptions.mWaterSmoothstepAA, 0.0f, 1.0f);
        ImGui::SliderFloat("Color Noise Intensity", &sDebugOptions.mWaterColorNoiseIntensity, 0.0f, 1.0f);
        ImGui::SliderFloat("Distort Tiling", &sDebugOptions.mWaterDistortTiling, 0.0f, 16.0f);
        ImGui::SliderFloat("Noise Tiling", &sDebugOptions.mWaterNoiseTiling, 0.0f, 16.0f);
        ImGui::DragFloatRange2("Foam Dist Range", &sDebugOptions.mWaterFoamDistanceRange.x, &sDebugOptions.mWaterFoamDistanceRange.y, 0.01f, 0.0f, 2.0f);
        ImGui::PopID();
        ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Lighting")) {
        ImGui::PushID(++ID);
        ImGui::Checkbox("Split View", &sDebugOptions.mLightPresetSplitView);
        if (sDebugOptions.mLightPresetSplitView) {
            ImGui::SliderFloat("Split Line", &sDebugOptions.mLightPresetSplitAmount, 0.0f, 1.0f);
            ImGui::Separator();
            ImGui::Text("LEFT: "); ImGui::SameLine();
            ImGui::Text(LIGHT_PRESET_NAMES[sDebugOptions.mLightingPreset]);
            if (ImGui::SliderInt("Light Preset Left", &sDebugOptions.mLightingPreset, 0, LIGHT_PRESET_COUNT - 1)) {
                sDebugOptions.mLightingOptions = &sLightingPresets[sDebugOptions.mLightingPreset];
            }
            renderLightingUI(ID, sDebugOptions.mLightingOptions, sDebugOptions.mLightingPreset);
            ImGui::Separator();
            ImGui::Text("RIGHT: "); ImGui::SameLine();
            ImGui::Text(LIGHT_PRESET_NAMES[sDebugOptions.mLightingPresetSplit]);
            if (ImGui::SliderInt("Light Preset Right", &sDebugOptions.mLightingPresetSplit, 0, LIGHT_PRESET_COUNT - 1)) {
                sDebugOptions.mLightingOptionsSplit = &sLightingPresets[sDebugOptions.mLightingPresetSplit];
            }
            renderLightingUI(ID, sDebugOptions.mLightingOptionsSplit, sDebugOptions.mLightingPresetSplit);
            if (sDebugOptions.mLightingPresetSplit != LIGHT_PRESET_CUSTOM) {
                if (ImGui::Button("Copy to CUSTOM")) {
                    sLightingPresets[LIGHT_PRESET_CUSTOM] = sLightingPresets[sDebugOptions.mLightingPresetSplit];
                }
            }
            if (ImGui::Button("Swap left/right")) {
                std::swap(sDebugOptions.mLightingPreset, sDebugOptions.mLightingPresetSplit);
                std::swap(sDebugOptions.mLightingOptions, sDebugOptions.mLightingOptionsSplit);
            }
            ImGui::Separator();
        }
        else {
            ImGui::Text(LIGHT_PRESET_NAMES[sDebugOptions.mLightingPreset]);
            if (ImGui::SliderInt("Light Preset", &sDebugOptions.mLightingPreset, 0, LIGHT_PRESET_COUNT - 1)) {
                sDebugOptions.mLightingOptions = &sLightingPresets[sDebugOptions.mLightingPreset];
            }
            renderLightingUI(ID, sDebugOptions.mLightingOptions, sDebugOptions.mLightingPreset);
            ImGui::Separator();
        }
        
        ImGui::PopID();
        ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Tonemap Uchimura")) {
        ImGui::SliderFloat("Max Display Brightness", &sDebugOptions.unUchMaxDisplayBrightness, 0.0f, 2.0f);
        ImGui::SliderFloat("Contrast", &sDebugOptions.unUchContrast, 0.0f, 2.0f);
        ImGui::SliderFloat("Linear Section Start", &sDebugOptions.unUchLinearSectionStart, 0.0f, 1.0f);
        ImGui::SliderFloat("Linear Section Length", &sDebugOptions.unUchLinearSectionLength, 0.0f, 1.0f);
        ImGui::SliderFloat("Black", &sDebugOptions.unUchBlack, 0.0f, 2.0f);
        ImGui::SliderFloat("Pedestal", &sDebugOptions.unUchPedestal, 0.0f, 1.0f);
        ImGui::Separator();
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
        if (ImGui::RadioButton("Mouselock", sDebugOptions.mCameraMode == CameraMode::MOUSELOCK)) {
            sDebugOptions.mCameraMode = CameraMode::MOUSELOCK;
        }
        if (ImGui::RadioButton("Cartesian", sDebugOptions.mCameraMode == CameraMode::CARTESIAN)) {
            sDebugOptions.mCameraMode = CameraMode::CARTESIAN;
        }
        if (ImGui::RadioButton("MMO", sDebugOptions.mCameraMode == CameraMode::MMO)) {
            sDebugOptions.mCameraMode = CameraMode::MMO;
        }
        if (ImGui::RadioButton("First Person", sDebugOptions.mCameraMode == CameraMode::FIRST_PERSON)) {
            sDebugOptions.mCameraMode = CameraMode::FIRST_PERSON;
        }
        static_assert(e_cast(CameraMode::COUNT) == 6, "Update options");
        ImGui::PopID();
        ImGui::Separator();
    }
    
    if (ImGui::CollapsingHeader("Clouds")) {
        ImGui::PushID(++ID);
        ImGui::Checkbox("Debug rendering", &sDebugOptions.mDebugClouds);
        ImGui::Checkbox("Disable", &sDebugOptions.mDisableClouds);
        ImGui::SliderInt("Blur Passes", &sDebugOptions.mCloudBlurPasses, 0, 15);
        ImGui::SliderFloat("Blur Radius", &sDebugOptions.mCloudBlurRadius, 0.0f, 15.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Ambient", &sDebugOptions.mCloudAmbient, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("Speed", &sDebugOptions.mCloudSpeed, 0.0f, 50.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::PopID();
        ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Depth of Field")) {
        ImGui::PushID(++ID);
        ImGui::SliderInt("Blur Passes", &sDebugOptions.mDepthOfFieldBlurPasses, 0, 15);
        ImGui::SliderFloat("Blur Radius", &sDebugOptions.mDepthOfFieldBlurRadius, 0.0f, 15.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Exponent", &sDebugOptions.mDepthOfFieldExponent, 0.0f, 2.0f, "%.3f");
        ImGui::DragFloatRange2("Blur Range Near", &sDebugOptions.mDepthOfFieldRangeNear.x, &sDebugOptions.mDepthOfFieldRangeNear.y, 0.01f, 0.0f, 3.0f);
        ImGui::DragFloatRange2("Blur Range Far", &sDebugOptions.mDepthOfFieldRangeFar.x, &sDebugOptions.mDepthOfFieldRangeFar.y, 1.0f, 0.0f, 8000.0f, "%.3f", (const char*)0, ImGuiSliderFlags_Logarithmic);
        ImGui::Checkbox("DebugRender", &sDebugOptions.mDepthOfFieldDebugRender);
        ImGui::PopID();
        ImGui::Separator();
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
        ImGui::Separator();
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
        ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Toggles")) {
        ImGui::Checkbox("Pause Frustum Updates", &sDebugOptions.mPauseFrustum);
        ImGui::Checkbox("Show Wireframe", &sDebugOptions.mWireframe);
        bool chunkBoundaries = sDebugOptions.mChunkBoundaries;
        if (ImGui::Checkbox("Show Chunk Boundaries", &chunkBoundaries)) {
            sDebugOptions.mChunkBoundaries = chunkBoundaries;
        }
        ImGui::Checkbox("Show City Debug", &sDebugOptions.mCities);
        ImGui::Checkbox("Show Roof Debug", &sDebugOptions.mRoofDebug);
        ImGui::Checkbox("Show Navgraph", &sDebugOptions.mShowNavGraph);
        ImGui::Checkbox("Show Navgraph Updates", &sDebugOptions.mShowNavGraphUpdates);
        ImGui::Checkbox("Show Business Debug", &sDebugOptions.mShowBusinessDebug);
        ImGui::Checkbox("Show Paths", &sDebugOptions.mShowPaths);
        ImGui::Checkbox("Show Entity Queries", &sDebugOptions.mShowEntityQueries);
        ImGui::Checkbox("Show Dev Hud", &sDebugOptions.mShowDevHud);
        ImGui::Checkbox("Hide Models", &sDebugOptions.mHideModels);
        ImGui::Checkbox("Hide Characters", &sDebugOptions.mHideCharacters);
        ImGui::Separator();
        ImGui::Text("Physics Debug");
        ImGui::Checkbox("Static Physics (Toggle to refresh)", &sDebugOptions.mShowStaticPhysics);
        ImGui::Checkbox("Terrain Physics (Toggle to refresh)", &sDebugOptions.mShowTerrainPhysics);
        ImGui::Checkbox("Dynamic Physics", &sDebugOptions.mShowDynamicPhysics);
        ImGui::Checkbox("Actions (Characters)", &sDebugOptions.mShowPhysicsActions);

        ImGui::Separator();
    }
    if (activeGBuffer) {
        if (ImGui::CollapsingHeader("GBuffer")) {
            const ImVec2 uv0(0, 1);
            const ImVec2 uv1(1, 0);
            const ImVec2 dims(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().x / aspectRatio);
            ImGui::Text("Geometry");
            ImGui::Image((ImTextureID)activeGBuffer->getGeometryTexture(), dims, uv0, uv1);
            ImGui::Text("Normals");
            ImGui::Image((ImTextureID)activeGBuffer->getNormalTexture(), dims, uv0, uv1);
            ImGui::Text("Roughness");
            ImGui::Image((ImTextureID)activeGBuffer->getRoughnessTexture(), dims, uv0, uv1);
            ImGui::Text("Depth");
            ImGui::Image((ImTextureID)activeGBuffer->getDepthTexture(), dims, uv0, uv1);
            ImGui::Separator();
        }
    }

    if (ImGui::CollapsingHeader("Shader Tweaker")) {
        ImGui::PushID(++ID);
        ImGui::ColorPicker3("Debug Color 1", &sDebugOptions.mDebugColor01.x, ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar);
        ImGui::ColorPicker3("Debug Color 2", &sDebugOptions.mDebugColor02.x, ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar);
        ImGui::SliderFloat("DebugFloat1", &sDebugOptions.mDebugFloat01, 0.0f, 1.0f);
        ImGui::SliderFloat("DebugFloat2", &sDebugOptions.mDebugFloat02, 0.0f, 1.0f);
        ImGui::SliderFloat("DebugFloat3", &sDebugOptions.mDebugFloat03, 0.0f, 1.0f);
        ImGui::SliderFloat("DebugFloat4", &sDebugOptions.mDebugFloat04, 0.0f, 1.0f);
        ImGui::PopID();
        ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Visual Logs")) {
        VisualLogger::renderImgui();
        ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Value Tweaker")) {
        ImGui::PushID(++ID);
        renderTweakerImgui();
        ImGui::PopID();
        ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Animation Debugger")) {
        ImGui::PushID(++ID);
        ImGui::Text("PLEASE FIX ImGui::CollapsingHeader(\"Animation Debugger\")");
        //CharacterModelComponent& playerModel = ecs.mRegistry.get<CharacterModelComponent>(ecs.getLocalPlayer());
        //ui32 numActive = 0;
        //for (int i = 0; i < NUM_ANIM_STATE_TRACKS; ++i) {
        //    AnimTrack& track = playerModel.mAnimState->mTracks[i];
        //    const ozz::animation::Animation* anim = playerModel.mModel->mAnimMachine->mAnimsArray[i];
        //    if (anim) {
        //        bool isActive = track.mFlags.isBitSet(AnimTrackFlags::IS_ACTIVE);
        //        if (ImGui::Checkbox((nString("Is Active ") + std::to_string(i)).c_str(), &isActive)) {
        //            if (isActive) {
        //                track.mFlags.setBit(AnimTrackFlags::IS_ACTIVE);
        //            }
        //            else {
        //                track.mFlags.clearBit(AnimTrackFlags::IS_ACTIVE);
        //            }
        //        }
        //        if (track.isActive()) {
        //            ++numActive;
        //        }
        //        if (ImGui::SliderFloat(AnimMachineStateNames[i], &track.mWeightScale, 0.0f, 1.0f)) {
        //            // Debug update the context
        //            playerModel.setAnimTrackWeight(AnimMachineState(i), track.mWeight);
        //        }
        //        ImGui::SliderFloat((nString("Time ") + std::to_string(i)).c_str(), &track.mTime, 0.0f, track.mDuration);
        //        f32 fadeWeight = (f32)track.mWeight / MAX_ANIM_FADE_WEIGHT;
        //        ImGui::SliderFloat((nString("Weight ") + std::to_string(i)).c_str(), &fadeWeight, 0.0f, 1.0f);
        //        ImGui::Separator();
        //    }
        //}
        ImGui::Separator();
        /*ImGui::Text((nString("Total Active Anims: ") + std::to_string(numActive)).c_str());
        ImGui::SliderFloat((nString("Footstep alpha ")).c_str(), &playerModel.mFootstepAlpha, 0.0f, 1.0f);*/
        ImGui::PopID();
        ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Profiler")) {
        if (ImGui::Button("Reset Times")) {
            Instrumentor::get().resetTimes();
        }
        InstrumentorDebugOutputData timeStrings;
        Instrumentor::get().getDebugOutputData(timeStrings);
        std::map<nString, std::vector<InstrumentorDebugStrings>> sortedStrings;
        for (auto&& str : timeStrings.data) {
            sortedStrings[getThreadName(str.first)] = std::move(str.second);
        }
        for (auto&& str : sortedStrings) {
            if (ImGui::CollapsingHeader(str.first.c_str())) {
                for (InstrumentorDebugStrings& debugStr : str.second) {
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), debugStr.name.c_str());

                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 50, 255, 255));
                    ImGui::Text(debugStr.avg.c_str());
                    ImGui::Text(debugStr.max.c_str());
                    ImGui::PopStyleColor();
                }
            }
        }
        ImGui::Separator();
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
        ImGui::Separator();
        ImGui::Text("Extensions:");
        for (auto&& extension : sGlExtensions.sExtensions) {
            ImGui::Text(extension.c_str());
        }
        ImGui::Separator();
    }

    // Uncomment to learn imgui
    //bool show = true;
    //ImGui::ShowDemoWindow(&show);

    ImGui::EndChild();
}
