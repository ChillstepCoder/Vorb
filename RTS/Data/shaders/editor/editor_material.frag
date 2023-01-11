#include "MaterialData.glsl"
#include "GlobalUbo.glsl"
#include "lighting/scene_lighting.glsl"

uniform mat4 unVP;

in vec2 fUV;
in vec3 fWorldPos;
in vec2 fScreenPos;
in vec4 fTint;
in mat3 fTBN;

// 0 = default
// 1 = normalst
// 2 = uvs
uniform int unMaterialIndex;

#include "editor/editor_util.glsl"

layout (location = 0) out vec4 oColor;

void main() {

    vec4 color;
    vec3 normal;
    getMaterialPixelInfo(unMaterialIndex, fUV, color, normal, fTint);

    tryDiscardTransparentPixel(color.a);
    
    // Normal to tangent space
    normal = normalize(fTBN * normal);
    
    oColor = getEditorOutputPixelColor(color.rgb, normal, fWorldPos, fScreenPos);
}