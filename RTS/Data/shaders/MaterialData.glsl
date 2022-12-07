#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable

// This must not be modified, it is bound to code layout
struct MaterialData
{
	vec4 emissiveColor;
	vec4 albedoColor;
	vec4 roughness;

	float transparencyFactor;
	float alphaTest;
	float metallicFactor;

	uint  flags;

	uint64_t ambientOcclusionMap;
	uint64_t albedoMap;
	uint64_t metallicRoughnessMap;
	uint64_t normalMap;
};

layout(std430, binding = 1) restrict readonly buffer Materials
{
	MaterialData inMaterials[];
};