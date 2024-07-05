#pragma once
class PhysicsDebugger {
public:
    PhysicsDebugger();
    ~PhysicsDebugger();

    void updateAndRenderImGui(bool* pOpen);
};