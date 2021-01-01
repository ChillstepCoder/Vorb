uniform mat4 World;
uniform mat4 VP;
uniform float SunPosition;

in vec4 vPosition;
in vec2 vUV;
in vec4 vTint;
in float vAtlasPage;
in float vHeight;
in float vShadowState; // 0 = none, 1 = ALL, 2 = LEFT, 3 = RIGHT

out vec2 fUV;
flat out float fAtlasPage;
out float fShadowHeight;

const float SHADOW_XINTENSITY = 0.7;

void main() {
    fUV = vUV;
    fAtlasPage = vAtlasPage;
    vec4 worldPos = World * vPosition;
	
	float shadowEnabled = step(0.01, vShadowState);
	float shiftMult = 1.0;
	// To prevent branch, this overrides ALL to be always 1.0 shift
	float shiftMin = step(-1.01, -vShadowState);
	
	// 2 = left, 3 = right
	if (SunPosition > 0.0) {
		shiftMult = step(2.99, vShadowState); // 0 if left, 1 if right
	} else {
		shiftMult = step(-2.01, -vShadowState); // 1 if left, 0 if right
	}
	shiftMult = max(shiftMult, shiftMin);
	
    worldPos.x += worldPos.z * SunPosition * SHADOW_XINTENSITY * shadowEnabled * shiftMult;
    fShadowHeight = vHeight * shadowEnabled;
   
    gl_Position = VP * worldPos;
}