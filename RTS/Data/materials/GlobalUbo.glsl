// This must not be modified, it is bound to code layout
layout (std140, binding = 0) uniform GlobalUbo
{
    mat4 V;
	mat4 InverseV;
	mat4 P;
	mat4 InverseP;
	mat4 VP;
	mat4 InverseVP;
	vec3 SunPosition;
	vec3 SunPositionCameraRelative;
	vec3 SunUp;
	vec3 SunRight;
	vec3 SunColor;
	vec3 PlayerPosWorld;
	vec3 CameraPos;
	vec3 CameraFront;
	vec3 CameraRight;
	vec3 CameraUp;
	vec2 CameraZRange;
	float SunHeight;
	float Time;
	float TimeOfDay;
};