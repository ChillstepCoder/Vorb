#include "MaterialData.glsl"
#include "GlobalUbo.glsl"
#include "lighting/scene_lighting.glsl"

uniform mat4 unVP;

in vec2 fUV;
in vec3 fWorldPos;
in vec2 fScreenPos;
flat in uint fMaterialIndex;
in vec4 fTint;
in mat3 fTBN;
in vec3 fTangent;
in vec3 fNormal;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;

#include "editor/editor_util.glsl"

void main() {

    vec3 normal;
    float ao;
    float metallic;
    float roughness;
    getMaterialPixelInfo(fMaterialIndex, fUV, oColor, normal, ao, metallic, roughness, fTint);
    
    tryDiscardTransparentPixel(oColor.a);
	
	// Normal to tangent space
    normal = normalize(fTBN * normal);
    
    oColor = getEditorOutputPixelColor(oColor.rgb, normal, fTangent, ao, metallic, roughness, fWorldPos, fScreenPos);
    oNormal = (normal + 1.0) * 0.5;
}