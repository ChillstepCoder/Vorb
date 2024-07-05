#include "stdafx.h"
#include "PhysicsDebugger.h"

#include "world/World.h"
#include "physics/PhysicsWorld.h"

PhysicsDebugger::PhysicsDebugger() = default;
PhysicsDebugger::~PhysicsDebugger() = default;

void PhysicsDebugger::updateAndRenderImGui(bool* pOpen) {
    if (ImGui::Begin("Physics Debugger", pOpen, ImGuiWindowFlags_NoDocking)) {

        sGameWorld->getPhysicsWorld().updateAndRenderImguiDebugControls();

        ImGui::End();
    }
}
