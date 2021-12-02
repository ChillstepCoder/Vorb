#include "stdafx.h"
#include "DebugTweakerPanel.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include "options/DebugOptions.h"

DebugTweakerPanel::DebugTweakerPanel(const f32v2& screenDims) : mScreenDims(screenDims)
{
}

void DebugTweakerPanel::updateAndRender(const vg::GBuffer* activeGBuffer, float aspectRatio)
{
    constexpr float WINDOW_WIDTH = 400.0f;
    const float WINDOW_HEIGHT = mScreenDims.y;
    ImGui::SetNextWindowPos(ImVec2(mScreenDims.x - WINDOW_WIDTH, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, WINDOW_HEIGHT));
    const ImVec2 buttonSize(WINDOW_WIDTH, 25);

    ImGui::Begin("Value Tweaker", &sDebugOptions.mShowTweaker, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
    ui32 ID = 10;

    //PushOverrideID //POPID

    if (ImGui::CollapsingHeader("Game Settings")) {
        ImGui::Checkbox("VSYNC", &sDebugOptions.mVSYNC);
        if (ImGui::SliderFloat("Load range", &sDebugOptions.mLoadRange, 128.0f, 3000.0f, "%.1f")) {
            sDebugOptions.mLoadRangeSq = SQ(sDebugOptions.mLoadRange);
        }
    }

    if (ImGui::CollapsingHeader("Camera Settings")) {
        ImGui::SliderFloat("FoV", &sDebugOptions.mFoV, 1.0f, 179.0f, "%.1f");
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

    if (ImGui::CollapsingHeader("Shadows")) {
        ImGui::PushID(++ID);
        ImGui::Checkbox("Disable", &sDebugOptions.mDisableShadows);
        ImGui::SliderFloat("Z Mult", &sDebugOptions.mShadowZMult, 0.0f, 100.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Update Rate Seconds", &sDebugOptions.mShadowUpdateRateSeconds, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Near Cascade Size", &sDebugOptions.mShadowNearSize, 10.0f, 300.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::ColorPicker3("Color", &sDebugOptions.mShadowColor.x, ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar);
        ImGui::PopID();
    }

    if (ImGui::CollapsingHeader("Toggles")) {
        ImGui::Checkbox("Pause Frustum Updates", &sDebugOptions.mPauseFrustum);
        ImGui::Checkbox("Show Wireframe", &sDebugOptions.mWireframe);
        ImGui::Checkbox("Show Chunk Boundaries", &sDebugOptions.mChunkBoundaries);
        ImGui::Checkbox("Show City Debug", &sDebugOptions.mCities);
        ImGui::Checkbox("Show Navgraph Updates", &sDebugOptions.mNavGraph);
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
