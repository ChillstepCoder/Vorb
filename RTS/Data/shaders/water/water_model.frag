#include "MaterialData.glsl"
#include "GlobalUbo.glsl"


in vec3 fPosition;
in vec2 fUV;
in float fDepth;
in float fCameraDist;
flat in uint fMaterial;

#include "util/water.glsl"

layout (location = 0) out vec4 oColor;

void main() {
    MaterialData mtl = inMaterials[fMaterial];
    
    float depthDiff = getDepthDiff(FboDepth, ScreenResolution, CameraZRange);
    depthDiff *= 5.0;
    
    vec4 waterColor = sampleMaterialAlbedo(mtl, vec2(0.0, 1.0 - clamp(depthDiff, 0.0, 1.0))).rgba;
    oColor = computeWaterColor(fPosition, fUV, depthDiff, fCameraDist, waterColor);
}