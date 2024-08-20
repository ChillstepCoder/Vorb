
layout (std430, binding = 4) restrict readonly buffer ModelVariantData {
	uint inVariantMaterials[];
};

layout (std430, binding = 5) restrict readonly buffer SubmeshWindData {
	int inSubmeshWindData[];
};