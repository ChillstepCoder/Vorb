// Note that std140 pads this to a vec4
struct VariantData {
    uint material;
};
// Match MAX_MODEL_VARIANTS in C++ (16)
layout (std140, binding = 4) uniform ModelVariantData {
	VariantData inVariantData[16];
};