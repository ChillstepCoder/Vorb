#include "MaterialData.glsl"

in vec2 fUV;
flat in uint fMaterialIndex;
in mat3 fTBN;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;
layout (location = 2) out vec3 oRoughness;

void main() {
    MaterialData mtl = inMaterials[fMaterialIndex];
    vec4 color = sampleMaterialAlbedo(mtl, fUV);
	
    tryDiscardTransparentPixel(color.a);
    oColor.rgb = color.rgb;
    oColor.a = 1.0; // AO?
	
	
	// Normal is always the next page
    vec3 normal;
    if (mtl.normalMap > 0) {
        normal = sampleMaterialNormal(mtl, fUV).rgb;
        normal = normal * 2.0 - 1.0;
    } else {
        normal = vec3(0.0, 0.0, 1.0);
    }
    
	normal = normalize(fTBN * normal);
	oNormal.rgb = (normal + 1.0) * 0.5;
	//oRoughness.r = fRoughness;
    //oColor.rgb = oColor.rgb * 0.0001 + vec3(1.0, 0.0, 0.0);
}