#include "MaterialData.glsl"

in vec2 fUV;
flat in uint fParticleMaterial;
flat in vec4 fColor;
flat in uint fParticleId;

uniform float EmitterNormalizedLifetime;

out vec4 oColor;

layout(std430, binding = 10) readonly restrict buffer StartMaterial
{
    uint sourceMaterials[];
};

layout(std430, binding = 11) readonly restrict buffer EndMaterial
{
    uint targetMaterials[];
};

layout(std430, binding = 12) readonly restrict buffer MeshSourceUV
{
    vec2 sourceUVs[];
};

layout(std430, binding = 13) readonly restrict buffer MeshTargetUV
{
    vec2 targetUVs[];
};


void main() {
    MaterialData mtl = inMaterials[fParticleMaterial];
    // Expects greyscale sprite
    const float spriteAlpha = sampleMaterialAlbedo(mtl, fUV).r * fColor.a;
    
    
    // TODO: Profile as a vertex texture fetch instead of fragment
    
    vec3 sNormal;
    vec4 sColor;
    float sAo;
    float sMetallic;
    float sRoughness;
    getMaterialPixelInfo(sourceMaterials[fParticleId], sourceUVs[fParticleId], sColor, sNormal, sAo, sMetallic, sRoughness, vec4(1.0));
    
    vec3 tNormal;
    vec4 tColor;
    float tAo;
    float tMetallic;
    float tRoughness;
    getMaterialPixelInfo(targetMaterials[fParticleId], targetUVs[fParticleId], tColor, tNormal, tAo, tMetallic, tRoughness, vec4(1.0));
    
    oColor.rgba = mix(sColor, tColor, EmitterNormalizedLifetime);
    oColor.a *= spriteAlpha;
    
    tryDiscardTransparentPixel(oColor.a);
    oColor.a = 1.0;
}