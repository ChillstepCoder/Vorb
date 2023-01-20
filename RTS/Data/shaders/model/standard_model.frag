#include "MaterialData.glsl"

in vec2 fUV;
flat in uint fMaterialIndex;
in vec4 fTint;
in mat3 fTBN;

layout (location = 0) out vec3 oColor;
layout (location = 1) out vec3 oNormal;
layout (location = 2) out vec3 oRoughness;

void main() {

    vec3 normal;
    vec4 color;
    getMaterialPixelInfo(fMaterialIndex, fUV, color, normal, fTint);

    tryDiscardTransparentPixel(color.a);
	
	// Normal to tangent space
    normal = normalize(fTBN * normal);
    // Into 0-1 range
	oNormal = (normal + 1.0) * 0.5;
    
    
	oRoughness.r = 0.2;
    
    oColor = color.rgb;
}