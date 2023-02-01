#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable

#include "AlphaTest.glsl"

// This must not be modified, it is bound to code layout
struct MaterialData {
	vec4 emissiveColor;
	vec4 albedoColor;
	vec4 roughness;

	float transparencyFactor;
	float alphaTest;
	float metallicFactor;

	uint  flags;

	uint64_t albedoMap;
	uint64_t normalMap;
	uint64_t ambientOcclusionMap;
	uint64_t metallicRoughnessMap;
};

layout(std430, binding = 1) restrict readonly buffer Materials {
	MaterialData inMaterials[];
};

vec4 sampleMaterialAlbedo(MaterialData mtl, vec2 uv) {
    return texture( sampler2D(unpackUint2x32(mtl.albedoMap)), uv);
}

vec3 sampleMaterialNormal(MaterialData mtl, vec2 uv) {
    return texture( sampler2D(unpackUint2x32(mtl.normalMap)), uv).xyz;
}

float getMaterialRoughness(MaterialData mtl) {
    return mtl.roughness.r;
}

float getMaterialMetallic(MaterialData mtl) {
    return mtl.metallicFactor;
}

void getMaterialPixelInfo(uint materialIndex, vec2 uv, inout vec4 color, inout vec3 normal, inout float metallic, inout float roughness, vec4 tint) {
    MaterialData mtl = inMaterials[materialIndex];
    
    color = mtl.albedoColor;
	normal = vec3(0.0, 0.0, 1.0);
    metallic = getMaterialMetallic(mtl);
    roughness = getMaterialRoughness(mtl);

	if (mtl.albedoMap > 0) {
		color = sampleMaterialAlbedo(mtl, uv);
    }
	if (mtl.normalMap > 0) {
		normal = sampleMaterialNormal(mtl, uv);
        normal = normal * 2.0 - 1.0;
    }
    color = color * tint;
}

void tryDiscardTransparentPixel(float alpha) {
    runAlphaTest(alpha, 0.01);
    
}