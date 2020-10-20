uniform mat4 World;
uniform mat4 VP;
uniform float SunPosition;

in vec4 vPosition;
in vec2 vUV;
in vec4 vTint;
in float vAtlasPage;
in float vHeight;
in float vShadowEnabled;

out vec2 fUV;
flat out float fAtlasPage;
out float fShadowHeight;

const float SHADOW_XINTENSITY = 0.7;

void main() {
    fUV = vUV;
    fAtlasPage = vAtlasPage;
    vec4 worldPos = World * vPosition;
	
    worldPos.x += worldPos.z * SunPosition * SHADOW_XINTENSITY * vShadowEnabled;
    fShadowHeight = vHeight * vShadowEnabled;
   
    gl_Position = VP * worldPos;
}