uniform sampler2D unAlbedoFbo;
uniform sampler2D unNormalFbo;
uniform sampler2D unDepthFbo;
uniform sampler2D PerlinNoise;
//uniform vec2 unScreenResolution;
//uniform vec2 unDirection;
//uniform vec2 unCameraZRange;

#include "GlobalUbo.glsl"
#include "util/depth.glsl"

//vec3 worldPosFromDepth(float depth, vec2 fboUV, mat4 inverseV, mat4 inverseP)

in vec2 fUV;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;


void main() {

    const vec3 worldPos = worldPosFromDepth(texture(unDepthFbo, fUV).r, fUV, InverseV, InverseP);
    
    float noiseColor = texture(PerlinNoise, worldPos.xy).r;
    
    vec3 baseNormal = texture(unNormalFbo, fUV).rgb;
    vec4 baseColor = texture(unAlbedoFbo, fUV);
    
    oColor.r = fract(worldPos.x);
    oColor.g = noiseColor;
    oColor.b = baseColor.b;
    oNormal.rgb = baseNormal;
}