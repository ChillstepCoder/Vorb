#include "GlobalUbo.glsl"

uniform vec4 unShallowColor;
uniform vec4 unDeepColor;

in vec3 fPosition;
in vec2 fUV;
in float fDepth;
in float fCameraDist;
#include "util/water.glsl"

layout (location = 0) out vec4 oColor;

void main() {
    float depthDiff = getDepthDiff(FboDepth, ScreenResolution, CameraZRange);

    vec4 waterColor = mix(unShallowColor, unDeepColor, clamp(depthDiff, 0.0, 1.0));
    oColor = computeWaterColor(fPosition, fUV, depthDiff, fCameraDist, waterColor);
}