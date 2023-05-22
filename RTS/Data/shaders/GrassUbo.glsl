
const int NUM_GRASS_MATERIALS = 32;

// Match C++ layout
struct GrassData {
    float grassScaleX; // Split due to 140
    float grassScaleY;
    int material;
    int materialCellCount;
    int shouldUseColorGradient;
    float leanVariance;
};

layout (std140, binding = 9) uniform GrassUbo {
	GrassData unGrassData[NUM_GRASS_MATERIALS];
};