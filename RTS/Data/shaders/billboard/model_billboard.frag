#include "MaterialData.glsl"

in vec2 fUV;
flat in uint fMaterialIndex;
in mat3 fTBN;
flat in float fCrossfade;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;
layout (location = 2) out vec2 oMetallicRoughness;

// This must not be modified, it is bound to code layout
struct BillboardMaterialData {
	uint64_t albedoMap;
	uint64_t normalMap;
	uint64_t aoMetallicRoughnessMap;
};

layout(std430, binding = 4) restrict readonly buffer BillboardMaterials {
	BillboardMaterialData inBillboardMaterials[];
};

void main() {
    BillboardMaterialData mtl = inBillboardMaterials[fMaterialIndex];
	vec4 color =  texture( sampler2D(unpackUint2x32(mtl.albedoMap)), fUV);
    runAlphaTestWithCrossfade(color.a, fCrossfade);
	// Normal is always the next page
    vec3 normal = texture( sampler2D(unpackUint2x32(mtl.normalMap)), fUV).rgb;
    vec3 AMR = texture( sampler2D(unpackUint2x32(mtl.aoMetallicRoughnessMap)), fUV).rgb;
    
	normal = normalize(fTBN * normal);
    
	oNormal.rgb = (normal + 1.0) * 0.5;
    oColor.rgb = color.rgb;
    oColor.a = AMR.x;
    oMetallicRoughness.r = AMR.y;
	oMetallicRoughness.g = AMR.z;
}