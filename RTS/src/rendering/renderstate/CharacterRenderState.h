#pragma once

struct CharacterRenderState {
    entt::entity mEntityID;
    f32v3 mPos;
    f32 mRotation;
    CharacterLocomotionMode mLocomotionMode;
};
static_assert(sizeof(CharacterRenderState) == 24, "Keep small");