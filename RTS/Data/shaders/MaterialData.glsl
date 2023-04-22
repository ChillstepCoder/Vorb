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
	uint64_t displacementMap;
	uint64_t aoMetallicRoughnessMap;
};

layout(std430, binding = 1) restrict readonly buffer Materials {
	MaterialData inMaterials[];
};

vec4 sampleMaterialAlbedo(MaterialData mtl, vec2 uv) {
    return texture( sampler2D(unpackUint2x32(mtl.albedoMap)), uv) * mtl.albedoColor;
}

vec3 sampleMaterialNormal(MaterialData mtl, vec2 uv) {
    return texture( sampler2D(unpackUint2x32(mtl.normalMap)), uv).xyz;
}

float sampleMaterialDisplacement(MaterialData mtl, vec2 uv) {
    return texture( sampler2D(unpackUint2x32(mtl.displacementMap)), uv).x;
}

vec3 sampleMaterialAOMetallicRoughness(MaterialData mtl, vec2 uv) {
    vec3 aoMetallicRoughness = texture( sampler2D(unpackUint2x32(mtl.aoMetallicRoughnessMap)), uv).xyz;
    aoMetallicRoughness.y *= mtl.metallicFactor;
    aoMetallicRoughness.z *= mtl.roughness.r;
    return aoMetallicRoughness;
}

void getMaterialPixelInfo(uint materialIndex, vec2 uv, inout vec4 color, inout vec3 normal, inout float ao, inout float metallic, inout float roughness, vec4 tint) {
    MaterialData mtl = inMaterials[materialIndex];
    
	if (mtl.albedoMap > 0) {
		color = sampleMaterialAlbedo(mtl, uv);
    } else {
        color = mtl.albedoColor;
    }
	if (mtl.normalMap > 0) {
		normal = sampleMaterialNormal(mtl, uv);
        normal = normal * 2.0 - 1.0;
    } else {
        normal = vec3(0.0, 0.0, 1.0);
    }
    if (mtl.aoMetallicRoughnessMap > 0) {
        vec3 aoMetallicRoughness = sampleMaterialAOMetallicRoughness(mtl, uv);
        ao = aoMetallicRoughness.x;
        metallic = aoMetallicRoughness.y;
        roughness = aoMetallicRoughness.z;
    } else {
        ao = 1.0;
        metallic = mtl.metallicFactor;
        roughness = mtl.roughness.r;
    }
    color = color * tint;
}

void tryDiscardTransparentPixel(float alpha) {
    runAlphaTest(alpha, 0.01);
}

vec2 dispMapping(vec2 uvs, sampler2D disp, vec3 viewDirection, float heightScale) {
    // Variables that control parallax occlusion mapping quality
	const float minLayers = 4.0;
    const float maxLayers = 32.0; // 16.0 // 0.035 HEIGHT SCALE WORKS WELL WITH 16
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDirection)));
    numLayers = clamp(numLayers, minLayers, maxLayers);
	float layerDepth = 1.0 / numLayers;
	float currentLayerDepth = 0.0;
	
	// Remove the z division if you want less aberated results
	vec2 S = viewDirection.xy / viewDirection.z * heightScale; 
    vec2 deltaUVs = S / numLayers;
    
	
	vec2 UVs = uvs;
	float currentDepthMapValue = 1.0 - texture(disp, UVs).r;
	
	// Loop till the point on the heightmap is "hit"
	while(currentLayerDepth < currentDepthMapValue)
    {
        UVs -= deltaUVs;
        currentDepthMapValue = 1.0 - texture(disp, UVs).r;
        currentLayerDepth += layerDepth;
    }

	// Apply Occlusion (interpolation with prev value)
	vec2 prevTexCoords = UVs + deltaUVs;
	float afterDepth  = currentDepthMapValue - currentLayerDepth;
	float beforeDepth = 1.0 - texture(disp, prevTexCoords).r - currentLayerDepth + layerDepth;
	float weight = afterDepth / (afterDepth - beforeDepth);
	UVs = prevTexCoords * weight + UVs * (1.0 - weight);
    
    return UVs;
}