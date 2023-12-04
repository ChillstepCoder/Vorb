
// Optional inputs
uniform float unBiomeBlendScale = 1.0;
uniform float unBiomeBlendFrequency = 1.0;
uniform float unDetailTextureStrength = 1.0;

// .prog must define these inputs
uniform sampler2D TurbulentNoise;
uniform sampler2D CellNoise;

// Code must provide these inputs
uniform sampler2D unBiomeTexture;
uniform sampler2DArray unBiomeColorMapsTexture;
layout(std430, binding = 4) readonly buffer BiomeColorMapLookup
{
    uint biomeColorMapLookup[];
};

int getBiome(vec2 biomeUvs) {
    vec2 preturbUVs = biomeUvs * 400.0 * unBiomeBlendFrequency;
    vec2 biomePreturb;
    biomePreturb.x = texture(TurbulentNoise, preturbUVs).r * 2.0 - 1.0;
    biomePreturb.y = texture(TurbulentNoise, -preturbUVs.yx).r * 2.0 - 1.0;
    biomePreturb = biomePreturb * 0.00025 * unBiomeBlendScale;
    
    // Texture and color
    vec2 preturbedUV = biomeUvs + biomePreturb;
    return int(round(texture(unBiomeTexture, preturbedUV).r * 255.0));
}

float getBiomeColorGradientUCoord(vec2 terrainUvs) {
    float cellNoiseColor = texture(CellNoise, terrainUvs).r;
    return 1.0 - cellNoiseColor;
}
