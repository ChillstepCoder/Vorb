#include "stdafx.h"
#include "WorldEditor.h"

#include "Camera2D.h"
#include "world/WorldData.h"

#include <Vorb/ui/InputDispatcher.h>

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>


WorldEditor::WorldEditor(World& world, SDL_Window* window, Camera2D& camera) : mWorld(world), mWindow(window), mCamera(camera) {

}

void WorldEditor::init()
{
    // Camera
    mCamera.setScale(50.0f);
    mCamera.setPosition(WorldData::WORLD_CENTER);

    // Mouse control
    vui::InputDispatcher::mouse.onWheel.addFunctor([this](Sender sender, const vui::MouseWheelEvent& event) {
        mTargetScale = glm::clamp(mTargetScale + event.dy * mTargetScale * 0.2f, 0.03f, 1020.f);
    });

    vui::InputDispatcher::mouse.onMotion.addFunctor([this](Sender sender, const vui::MouseMotionEvent& event) {
        // Middle click or for when on touchpad, z key
        if (vui::InputDispatcher::mouse.isButtonPressed(vorb::ui::MouseButton::MIDDLE) ||
            vui::InputDispatcher::key.isKeyPressed(VKEY_Z)) {
            const float scale = (1.0f / mCamera.getScale());
            const f32v2 moveDir(-event.dx * scale, event.dy * scale);
            const f32v2& oldPos = mCamera.getPosition();
            mCamera.setPosition(oldPos + moveDir);
        }
    });
}

// TODO: Data
bool show_demo_window = true;
bool show_another_window = false;
float clear_color[3] = { 1.0f, 1.0f, 1.0f };
void WorldEditor::updateAndRender() {

    updateCamera();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(mWindow);
    ImGui::NewFrame();

    //// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &show_another_window);

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void WorldEditor::updateCamera()
{
    // Update camera zoom
    if (abs(mTargetScale - mScale) > 0.001f) {
        mScale = vmath::lerp(mScale, mTargetScale, 0.3f);
        mCamera.setScale(mScale);
    }
}
