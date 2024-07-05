#include "stdafx.h"
#include "GameplayDebugger.h"

#include "world/World.h"
#include "ecs/IFullECS.h"

#include "ui/debugging/PhysicsDebugger.h"

#include "ui/UIContext.h"
#include "ui/debugging/MovementDebugger.h"

#include "debugging/DebugRenderer.h"
#include "item/ItemRepository.h"

#include "ecs/component/NavigationComponent.h"

#include "options/DebugOptions.h"

#include <imgui/imgui_internal.h>

constexpr ui32 AI_DEBUG_UPDATE_CODE = 2532;


GameplayDebugger::GameplayDebugger() {
    if (sInstance) {
        panic("GameplayDebugger already exists");
    }
    sInstance = this;

    mMovementDebugger = std::make_unique<MovementDebugger>();
    mPhysicsDebugger = std::make_unique<PhysicsDebugger>();
}

GameplayDebugger::~GameplayDebugger() {
    sInstance = nullptr;
}


void GameplayDebugger::updateAndRenderImGui(const Camera3D& camera) {
    ui32v2 windowDims = UIContext::getInstance().getWindowDims();
    ImGui::SetNextWindowSize(ImVec2(windowDims.x, 0));
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    if (ImGui::Begin("Gameplay Debugger", 0, 
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Debug Panels")) {
                ImGui::Checkbox("AI", &sGameplayDebugOptions.showAIDebugger);
                ImGui::Checkbox("Physics", &sGameplayDebugOptions.showPhysicsDebugger);
                ImGui::Checkbox("Movement", &sGameplayDebugOptions.showMovementDebugger);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        ImGui::End();
    }

    if (sGameplayDebugOptions.showAIDebugger) {
        mAIDebugger.updateAndRenderImGui(camera);
    }

    if (sGameplayDebugOptions.showPhysicsDebugger) {
        mPhysicsDebugger->updateAndRenderImGui(&sGameplayDebugOptions.showPhysicsDebugger);
    }

    if (sGameplayDebugOptions.showMovementDebugger) {
        mMovementDebugger->updateAndRenderImGui(&sGameplayDebugOptions.showMovementDebugger);
    }
}

AIDebugger::AIDebugger() = default;

AIDebugger::~AIDebugger() = default;

void AIDebugger::updateAndRenderImGui(const Camera3D& camera) {
    if (ImGui::Begin("AI Debugger", &sGameplayDebugOptions.showAIDebugger, ImGuiWindowFlags_NoDocking)) {
        ImGui::Spacing();
        ImGui::Checkbox("Debug Paths", &sDebugOptions.mShowPaths);
        ImGui::Checkbox("Debug Sim Characters", &sDebugOptions.mDebugSimCharacters);
        ImGui::SeparatorText("Characters");
        if (ImGui::Button("Debug Closest Character")) {
            AIDebugEntityUpdateHandlePtr handle =
                std::make_shared<AIDebugEntityUpdateHandle>(AI_DEBUG_UPDATE_CODE,
                    [](entt::entity entity, entt::registry& registry, IAttachedEntityUpdateHandle* thisHandle) {
                        ASSERT_GAME_THREAD();

                        f32v3 pos = registry.get<PositionComponent>(entity).mPosition;
                        const auto& character = registry.get<DualCharacterComponent>(entity);
                        const auto& taskQueue = registry.get<DualTaskQueueComponent>(entity);
                        const auto& residency = registry.get<DualResidentComponent>(entity);
                        const auto bundle = registry.try_get<DualResourceBundleComponent>(entity);
                        const auto& nav = registry.get<NavigationComponent>(entity);

                        AIDebugEntityUpdateHandle* updateHandle = static_cast<AIDebugEntityUpdateHandle*>(thisHandle);
                        updateHandle->doThreadSafe(

                            // Copy data for AIDebugger
                            [&](AIDebugData& debugData) {
                                // Tasks
                                debugData.tasksDebugString = "=== TASK QUEUE ===\n";
                                for (size_t i = 0; i < taskQueue.taskQueue.size(); ++i) {
                                    debugData.tasksDebugString += std::to_string(i) + ": "
                                        + taskQueue.taskQueue[i].getDebugString();
                                    debugData.tasksDebugString += "\n";
                                }
                                debugData.position = pos;
                                debugData.homeId = residency.homeId;
                                if (bundle) {
                                    debugData.bundle = bundle->itemStack;
                                }
                                else {
                                    debugData.bundle = SimpleItemStack();
                                }
                                if (debugData.showPaths) {
                                    debugData.finePath = nav.getFinePathHandle();
                                    debugData.currentFineNode = nav.getCurrentFinePoint();
                                    debugData.coarsePath = nav.getCoarsePathHandle();
                                    debugData.currentCoarseNode = nav.getCurrentCoarsePoint();
                                }
                            }
                        );
                    }
                );
            sGameWorld->getECS().addThreadSafeEntityUpdateForNearestCharacter(handle, camera.getPosition());
            mEntityUpdateHandles.emplace_back(std::move(handle));
        }
        if (mEntityUpdateHandles.size()) {
            if (ImGui::Button("Clear All")) {
                mEntityUpdateHandles.clear();
            }
        }
        ImGui::End();
    }

    const i32v2 avail(UIContext::getWindowDims());
    constexpr i32v2 MAX_CELL_DIMS(400, 300);
    constexpr i32 CELL_PADDING = 2;
    const i32v2 maxCellCountsBeforeShrink(avail / MAX_CELL_DIMS);
    const i32v2 cellDims = MAX_CELL_DIMS; // TODO: Shrink when there are too many
    size_t i = 0;
    for (auto it = mEntityUpdateHandles.begin(); it != mEntityUpdateHandles.end();) {
        AIDebugEntityUpdateHandlePtr& handle = *it;
        AIDebugData data = handle->getDataCopy();
        AM::DebugRenderer::drawWireQuad(data.position - f32v3(0.5f, 0.5f, 0.0f), f32v2(1.0f), color::LightGreen);
        ImGui::PushID(i);

        const i32v2 cellCoords(i % maxCellCountsBeforeShrink.x, i / maxCellCountsBeforeShrink.x);
        const i32v2 screenPos = cellCoords * cellDims;
        ImGui::SetNextWindowSize(ImVec2(cellDims.x - CELL_PADDING, cellDims.y - CELL_PADDING));
        ImGui::SetNextWindowPos(ImVec2(screenPos.x, screenPos.y));

        bool isOpen = true;

        f32v3 screenPoint = camera.worldToScreenPoint(data.position);

        // ===================================================================
        // Floater widget
        // ===================================================================
        bool didPushStyle = false;
        if (screenPoint.z <= 0.0f || screenPoint.z >= 1.0f) {
            // Indicate offscreen via changing window color
            const ImVec4 backgroundColor = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, backgroundColor);
            didPushStyle = true;
        }
        else {
            f32v2 screenPosAttach = ((f32v2(screenPos) + f32v2(0.5f * cellDims.x, cellDims.y)) / f32v2(avail)) * 2.0f - 1.0f;
            screenPosAttach.y = -screenPosAttach.y;
            const f32v3 pickRay = camera.getPickRayWorldSpace(screenPosAttach) * 10.0f;
            AM::DebugRenderer::drawLineBetweenPoints(data.position, camera.getPosition() + pickRay, color::LightGreen);
        }


        // ===================================================================
        // Debug Header
        // ===================================================================
        ImGui::Begin(std::format("Entity {}", i).c_str(), &isOpen, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoResize);
        ImGui::Text("Pos %s", data.tasksDebugString.c_str(),
            data.position.x, data.position.y, data.position.z);

        // ===================================================================
        // Debug Paths
        // ===================================================================
        if (ImGui::Checkbox("Show Path", &data.showPaths)) {
            handle->doThreadSafe(
                [&](AIDebugData& debugData) {
                    debugData.showPaths = data.showPaths;
                }
            );
        }

        if (data.showPaths) {
            constexpr f32 POINT_RADIUS = 0.1f;
            if (data.finePath) {
                for (i32 i = 1; i < data.finePath->getNumPoints(); ++i) {
                    if (i >= data.currentFineNode) {
                        AM::DebugRenderer::drawLineBetweenPoints(data.finePath->getPoint(i - 1), data.finePath->getPoint(i), color::LightGreen);
                        AM::DebugRenderer::drawFilledQuad(
                            data.finePath->getPoint(i) - f32v3(POINT_RADIUS, POINT_RADIUS, 0.0f),
                            f32v2(POINT_RADIUS * 2.0f, POINT_RADIUS * 2.0f), color::LightGreen);
                    }
                    else {
                        AM::DebugRenderer::drawLineBetweenPoints(data.finePath->getPoint(i - 1), data.finePath->getPoint(i), color::Red);
                        AM::DebugRenderer::drawFilledQuad(
                            data.finePath->getPoint(i) - f32v3(POINT_RADIUS, POINT_RADIUS, 0.0f),
                            f32v2(POINT_RADIUS * 2.0f, POINT_RADIUS * 2.0f), color::Red);
                    }
                }
            }

            if (data.coarsePath) {
                for (i32 i = 1; i < data.coarsePath->getNumPoints(); ++i) {
                    if (i >= data.currentCoarseNode) {
                        AM::DebugRenderer::drawLineBetweenPoints(data.coarsePath->getPoint(i - 1), data.coarsePath->getPoint(i), color4(0.0f, 1.0f, 0.0f, 0.5f));
                        AM::DebugRenderer::drawFilledQuad(
                            data.coarsePath->getPoint(i) - f32v3(POINT_RADIUS, POINT_RADIUS, 0.0f),
                            f32v2(POINT_RADIUS * 2.0f, POINT_RADIUS * 2.0f), color4(0.0f, 1.0f, 0.0f, 0.5f));
                    }
                    else {
                        AM::DebugRenderer::drawLineBetweenPoints(data.coarsePath->getPoint(i - 1), data.coarsePath->getPoint(i), color4(1.0f, 0.0f, 0.0f, 0.5f));
                        AM::DebugRenderer::drawFilledQuad(
                            data.coarsePath->getPoint(i) - f32v3(POINT_RADIUS, POINT_RADIUS, 0.0f),
                            f32v2(POINT_RADIUS * 2.0f, POINT_RADIUS * 2.0f), color4(1.0f, 0.0f, 0.0f, 0.5f));
                    }
                }
            }
        }

        // ===================================================================
        // Debug Home
        // ===================================================================
        if (data.homeId == INVALID_BUILDING_ID) {
            ImGui::Text("Home: INVALID");
        }
        else {
            ImGui::Text("Home: %d", data.homeId);
        }


        // ===================================================================
        // Debug Bundle
        // ===================================================================
        if (data.bundle.count) {
            ImGui::Text("Bundle: %s - %d",
                ItemRepository::get().getAssetName(data.bundle.itemId).toString().c_str(),
                data.bundle.count
            );
        }
        ImGui::End();

        if (didPushStyle) {
            ImGui::PopStyleColor();
        }

        if (!isOpen) {
            it = mEntityUpdateHandles.erase(it);
        }
        else {
            ++it;
        }

        ++i;
        ImGui::PopID();
    }
}
