#pragma once

// Must match layout of GlobalUbo.glsl
// Padding to match the required boundaries for each type https://learnopengl.com/Advanced-OpenGL/Advanced-GLSL
struct GlobalUboData {
    f32v3 SunPosition;
    f32 pad0; //padding
    f32v3 SunPositionCameraRelative;
    f32 pad1; //padding
    f32v3 SunUp;
    f32 pad2; //padding
    f32v3 SunRight;
    f32 pad3; //padding
    f32v3 SunColor;
    f32 pad4; //padding
    f32v3 PlayerPosWorld;
    float SunHeight;
    float Time;
    float TimeOfDay;
};

struct CameraUboData {
    // These are copied into the buffer directly from Camera3d.h so we don't need any extra matrix copies
 //  ****************
 // f32m4 V;
 // f32m4 InverseV;
 // f32m4 P;
 // f32m4 InverseP;
 // f32m4 VP;
 // f32m4 InverseVP;
 //  ****************
    f32v3 CameraPos;
    f32 pad0; //padding
    f32v3 CameraFront;
    f32 pad1; //padding
    f32v3 CameraRight;
    f32 pad2; //padding
    f32v3 CameraUp;
    f32 pad3; //padding
    f32v2 CameraZRange;
};