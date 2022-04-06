#include "../GlobalUbo.glsl"

uniform vec3 DebugColor1;
uniform sampler2D FboDepth;

in vec3 fPosition;
in vec2 fUV;
in float fDepth;
in float fWaveHeight;

layout (location = 0) out vec4 oColor; // TODO: vec3
layout (location = 1) out vec4 oNormal;
layout (location = 2) out vec4 oRoughness;

float linearizeDepth(float d) {
    float zn = 2.0 * d - 1.0;
    return 2.0 * CameraZRange.x * CameraZRange.y / (CameraZRange.y + CameraZRange.x - zn * (CameraZRange.y - CameraZRange.x));
}

void main() {
	
	float depth = texture2D(FboDepth, gl_FragCoord.xy).r;
    depth = linearizeDepth(depth);
    oColor.rg = vec2(depth);
    
    oColor.rgb = DebugColor1;
    oColor.r = fWaveHeight * 3.0;
    oColor.a = min(fDepth * 0.1 + 0.4, 0.8);
    oNormal.rgb = vec3(0.5, 0.5, 1.0);
    oNormal.a = 1.0;
    
    // Get depth value of current pixel
    float ndcDepth = (2.0 * gl_FragCoord.z - gl_DepthRange.near - gl_DepthRange.far) / (gl_DepthRange.diff);
	float clipDepth = ndcDepth / gl_FragCoord.w;
    
    float depthDiff = clipDepth - depth;
    if (depthDiff > 0) oColor.rgb = vec3(1.0);
    //oColor.rgb = oColor.rgb * 0.0001 + vec3(cos(Time + fPosition.x), sin(Time + fPosition.y), -cos(Time));
    oColor.rg = gl_FragCoord.xy;
	oRoughness.r = 0.0;
	oRoughness.a = 1.0;
}