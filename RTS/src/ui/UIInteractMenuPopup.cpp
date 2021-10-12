#include "stdafx.h"
#include "UIInteractMenuPopup.h"

#include "World.h"
#include "ecs/EntityComponentSystem.h"
#include "ecs/component/CharacterDetailsComponent.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>
//
//static bool view(NoiseFunction& n) {
//    bool changed = false;
//    ImGui::Text(n.label.c_str());
//    changed |= ImGui::SliderInt((n.label + "_octaves").c_str(), &n.octaves, 1, 15);
//    changed |= ImGui::SliderScalar((n.label + "_persist").c_str(), ImGuiDataType_Double, &n.persistence, &f64_zero, &f64_one);
//    changed |= ImGui::SliderScalar((n.label + "_freq").c_str(), ImGuiDataType_Double, &n.frequency, &f64_zero, &f64_one, "%.10f", ImGuiSliderFlags_Logarithmic);
//    changed |= ImGui::SliderScalarN((n.label + "_offset").c_str(), ImGuiDataType_Double, &(n.offset.x), 2, &MIN_NOISE_OFFSET, &MAX_NOISE_OFFSET, "%.10f");
//    return changed;
//}

UIInteractMenuPopup::UIInteractMenuPopup(const f32v2& screenPos, SDL_Window* window, WorldObjectQuery&& worldObjectQuery) :
    mScreenPos(screenPos),
    mWindow(window),
    mWorldObjectQuery(worldObjectQuery)
{

}

UIInteractMenuPopup::~UIInteractMenuPopup()
{

}

UIInteractMenuResultFlags UIInteractMenuPopup::updateAndRender()
{
    ui32 resultFlags = 0;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(mWindow);
    ImGui::NewFrame();
    const ImVec2 buttonSize(150, 25);
    
    ImGui::SetNextWindowPos(ImVec2(mScreenPos.x, mScreenPos.y));
    ImGui::SetNextWindowSize(ImVec2(buttonSize.x + 16, INTERACT_MENU_RESULT_COUNT * buttonSize.y + 45));

    World& world = mWorldObjectQuery.getWorld();

    switch (mState) {
        case UIInteractMenuState::SELECT_OBJECT: {
            ImGui::Begin("Select Object", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);

            int i = 1;
            int optionCount = 0;
            UIInteractMenuState nextState = UIInteractMenuState::SELECT_OBJECT; // Store best state in case we only have one so we can auto select

            // Agents
            for (auto& it : mWorldObjectQuery.getEntities()) {
                if (CharacterDetailsComponent* cmp = world.getECS().mRegistry.try_get<CharacterDetailsComponent>(it.second)) {
                    ++optionCount;
                    nextState = UIInteractMenuState::SELECTED_AGENT;
                    if (ImGui::Button((std::to_string(i++) + " " + cmp->name).c_str(), buttonSize)) {
                        mState = nextState;
                        break;
                    }
                }
            }

            // Stockpile
            if (mWorldObjectQuery.getStockpile()) {
                ++optionCount;
                nextState = UIInteractMenuState::SELECTED_STOCKPILE;
                if (ImGui::Button((std::to_string(i++) + " Stockpile").c_str(), buttonSize)) {
                    mState = nextState;
                    break;
                }
            }

            // Tile
            ++optionCount;
            nextState = UIInteractMenuState::SELECTED_TILE;
            if (ImGui::Button((std::to_string(i++) + " Tile").c_str(), buttonSize)) {
                mState = nextState;
                break;
            }

            if (optionCount == 0) {
                resultFlags = INTERACT_MENU_RESULT_INVALID;
            }
            else if (optionCount == 1) {
                // Auto select
                mState = nextState;
            }

            break;
        }
        case UIInteractMenuState::SELECTED_TILE: {
            ImGui::Begin("Tile action", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);

            if (ImGui::Button("Go Here", buttonSize)) {
                resultFlags |= INTERACT_MENU_RESULT_PATHFIND;
            }
            if (ImGui::Button("Inspect", buttonSize)) {
                resultFlags |= INTERACT_MENU_RESULT_INSPECT;
            }
            if (ImGui::Button("Clear Tile", buttonSize)) {
                resultFlags |= INTERACT_MENU_RESULT_CLEAR_TILE;
            }
            if (ImGui::Button("Plant Tree", buttonSize)) {
                resultFlags |= INTERACT_MENU_RESULT_PLANT_TREE;
            }
            if (ImGui::Button("Build Wall", buttonSize)) {
                resultFlags |= INTERACT_MENU_RESULT_BUILD_WALL;
            }
            break;
        }
        case UIInteractMenuState::SELECTED_STOCKPILE: {
            ImGui::Begin("Stockpile", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);

            if (ImGui::Button("DEBUG: Add 25 wood", buttonSize)) {
                resultFlags |= INTERACT_MENU_RESULT_DEBUG_ADD_25_WOOD;
            }
            break;
        }
        case UIInteractMenuState::SELECTED_AGENT: {
            ImGui::Begin("Agent", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);

            if (ImGui::Button("DEBUG: Kill Agent", buttonSize)) {
                resultFlags |= INTERACT_MENU_RESULT_DEBUG_KILL_AGENT;
            }
            break;
        }
        default:
            assert(false);
            break;

    }
    static_assert(INTERACT_MENU_RESULT_COUNT == 8, "update");
    static_assert(enum_cast(UIInteractMenuState::COUNT) == 4, "update");

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui::EndFrame();

    return static_cast<UIInteractMenuResultFlags>(resultFlags);
}
