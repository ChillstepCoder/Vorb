struct VariantData {
    uint material;
};

layout (std140, binding = 4) uniform ModelVariantData {
	VariantData inVariantData[8];
};