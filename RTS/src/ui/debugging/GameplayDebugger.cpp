#include "stdafx.h"
#include "GameplayDebugger.h"

#include "world/World.h"
#include "ecs/IFullECS.h"

#include "ui/UIContext.h"
#include "debugging/DebugRenderer.h"

constexpr ui32 AI_DEBUG_UPDATE_CODE = 2532;


static GameplayDebugger* sGameplayDebugger = nullptr;

GameplayDebugger::GameplayDebugger()
{
    if (sGameplayDebugger) {
        panic("GameplayDebugger already exists");
    }
    sGameplayDebugger = this;
}

GameplayDebugger::~GameplayDebugger() {
    sGameplayDebugger = nullptr;
}

GameplayDebugger* GameplayDebugger::tryGetInstance() {
    return sGameplayDebugger;
}

void GameplayDebugger::updateAndRenderImGui(const Camera3D& camera) {
    if (ImGui::Begin("Gameplay Debugger")) {
        ImGui::Text("Hello, world!");
        ImGui::Button("Button");
        ImGui::Checkbox("Show AI Debugger", &sGameplayDebugOptions.showAIDebugger);

        ImGui::End();
    }

    if (sGameplayDebugOptions.showAIDebugger) {
        mAIDebugger.updateAndRenderImGui(camera);
    }
}

AIDebugger::AIDebugger() = default;

AIDebugger::~AIDebugger() = default;

void AIDebugger::updateAndRenderImGui(const Camera3D& camera) {
    if (ImGui::Begin("AI Debugger")) {
        ImGui::Text("AI Debugger");
        ImGui::Checkbox("Show AI Debugger", &sGameplayDebugOptions.showAIDebugger);
        if (ImGui::Button("Debug Closest Character")) {
            AIDebugEntityUpdateHandlePtr handle =
                std::make_shared<AIDebugEntityUpdateHandle>(AI_DEBUG_UPDATE_CODE,
                    [](entt::entity entity, entt::registry& registry, IAttachedEntityUpdateHandle* thisHandle) {
                        ASSERT_GAME_THREAD();

                        f32v3 pos = registry.get<PositionComponent>(entity).mPosition;
                        auto& character = registry.get<DualCharacterComponent>(entity);

                        AIDebugEntityUpdateHandle* updateHandle = static_cast<AIDebugEntityUpdateHandle*>(thisHandle);
                        updateHandle->doThreadSafe(

                            // Copy data for AIDebugger
                            [&](AIDebugData& debugData) {
                                debugData.taskName = CStrToken("trololol");
                                debugData.position = pos;
                            }
                        );
                    }
                );
            sGameWorld->getECS().addThreadSafeEntityUpdateForNearestCharacter(handle, camera.getPosition());
            mEntityUpdateHandles.emplace_back(std::move(handle));
        }
        ImGui::End();
    }

    const i32v2 avail(UIContext::getWindowDims());
    constexpr i32v2 MAX_CELL_DIMS(100, 50);
    const i32v2 maxCellCountsBeforeShrink(avail / MAX_CELL_DIMS);
    const i32v2 cellDims = MAX_CELL_DIMS; // TODO: SHRINK
    size_t i = 0;
    for (auto it = mEntityUpdateHandles.begin(); it != mEntityUpdateHandles.end();) {
        AIDebugEntityUpdateHandlePtr& handle = *it;
        AIDebugData data = handle->getDataCopy();
        DebugRenderer::drawWireQuad(data.position, f32v2(1.0f), color::Bisque);
        ImGui::PushID(i++);

        const i32v2 cellCoords(i % maxCellCountsBeforeShrink.x, i / maxCellCountsBeforeShrink.x);
        const i32v2 screenPos = cellCoords * cellDims;
        ImGui::SetNextWindowSize(ImVec2(cellDims.x, cellDims.y));
        ImGui::SetNextWindowPos(ImVec2(screenPos.x, screenPos.y));

        bool isOpen = true;

        // Agent debug floater
        ImGui::Begin(std::format("Entity {}", i).c_str(), &isOpen, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Task: %s, (%f,%f,%f)", data.taskName.toString().c_str(),
            data.position.x, data.position.y, data.position.z);
        ImGui::End();


        if (!isOpen) {
            it = mEntityUpdateHandles.erase(it);
            continue;
        }
        else {
            ++it;
        }

        ImGui::PopID();
    }
}
