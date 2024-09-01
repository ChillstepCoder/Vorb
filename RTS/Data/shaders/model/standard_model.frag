#include "MaterialData.glsl"
#include "GlobalUbo.glsl"

#include "util/triplanar.glsl"

in vec2 fUV;
flat in uint fMaterialIndex;
in vec4 fTint;
in mat3 fTBN;
in vec3 fViewTangent;
in vec3 fFragPosTangent;
in float fSnow;
in float fDamage;
in vec3 fLocalPosition;
flat in float fCrossfade;

uniform sampler2D TurbulentNoise;

uniform float unHeightScale = 0.023;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;
layout (location = 2) out vec2 oMetallicRoughness;

uniform uint unDamageTexture = 0;

void main() {

    // Displacement
    vec2 uv = fUV;
    MaterialData mtl = inMaterials[fMaterialIndex];
    if (mtl.displacementMap > 0) {
        vec3 tangentViewDir = normalize(fViewTangent - fFragPosTangent);
        uv = dispMapping(uv, sampler2D(unpackUint2x32(mtl.displacementMap)), tangentViewDir, unHeightScale);
    }  

    vec3 normal;
    vec4 color;
    float ao;
    float metallic;
    float roughness;
    getMaterialPixelInfo(fMaterialIndex, uv, color, normal, ao, metallic, roughness, fTint);
    runAlphaTestWithCrossfade(color.a, fCrossfade);
	
	// Normal to tangent space
    normal = normalize(fTBN * normal);
    // Invert normals if away from camera
    if (!gl_FrontFacing) {
        
        // Doesnt quite work
        //vec3 frontNormal = normal;
       // vec3 backNormal = -normal;
        //float blendFactor = dot(normal, -tangentViewDir);
        //normal = mix(frontNormal, backNormal, blendFactor);
        normal = -normal;
   }
    // Into 0-1 range
    
    oColor.rgb = color.rgb;
    
    // =========== Snow ===========
    oColor.rgb = mix(oColor.rgb, vec3(1.0), min(fSnow * 4.0, 1.0));
    
    oColor.a = ao;
    
    if (fDamage > 0.0) {
        vec3 normalD[3];
        vec4 colorD[3];
        float aoD[3];
        float metallicD[3];
        float roughnessD[3];
        vec2 uvs[3];
        const float DAMAGE_UV_SCALE = 1.0f;
        uvs[0] = vec2(fLocalPosition.x, fLocalPosition.z) * DAMAGE_UV_SCALE; // X plane
        uvs[1] = vec2(fLocalPosition.y, fLocalPosition.z) * DAMAGE_UV_SCALE; // Y plane
        uvs[2] = vec2(fLocalPosition.x, fLocalPosition.y) * DAMAGE_UV_SCALE; // Z plane
        
    
        // TODO: TRIPLANAR UV
        for (int i = 0; i < 3; ++i) {
            getMaterialPixelInfo(unDamageTexture, uvs[i], colorD[i], normalD[i], aoD[i], metallicD[i], roughnessD[i], vec4(1.0));
        }
        
        vec3 weights = computeTriPlanarBlend(fTBN[2], getLuminance(colorD[0].rgb), getLuminance(colorD[1].rgb), 0.0, 1.0, 80.0);
        vec3 blendedColor = weights.x * colorD[0].rgb + weights.y * colorD[1].rgb + weights.z * colorD[2].rgb;
        vec3 blendedNormal = computeTriplanarNormal(fTBN[2], normalD, weights);
    
        float damageTextureAlpha = pow(fDamage, 0.2);
        oColor.rgb = mix(oColor.rgb, blendedColor, damageTextureAlpha);
        metallic = mix(metallic, metallicD[0], damageTextureAlpha);
        roughness = mix(roughness, roughnessD[0], damageTextureAlpha);
        normal = mix(normal, fTBN * blendedNormal, damageTextureAlpha);
        oColor.a = mix(oColor.a, aoD[0], damageTextureAlpha);
        
        const float BARK_THICKNESS = 0.07;
        float barkVal = max(BARK_THICKNESS - fDamage, 0.0) / BARK_THICKNESS;
        const vec3 BARK_COLOR = vec3(69.0/255.0, 48.0/255.0, 22.0/255.0);
        oColor.rgb = mix(oColor.rgb, BARK_COLOR, pow(barkVal, 0.75));
        
        const float CORE_THICKNESS = 0.1;
        if (fDamage > 1.0 - CORE_THICKNESS) {
            const float coreVal = (fDamage - (1.0 - CORE_THICKNESS)) / CORE_THICKNESS;
            const float SCALE = 1.0;
            const float TimeAnim = Time * 0.5;
            const float rawTurb = texture(TurbulentNoise, uvs[0] * SCALE + vec2(0.0, -TimeAnim)).r * weights.x + 
                                    texture(TurbulentNoise, uvs[1] * SCALE + vec2(0.0, -TimeAnim)).r * weights.y + 
                                    texture(TurbulentNoise, uvs[2] * SCALE + vec2(0.0, -TimeAnim)).r * weights.z;
            const float heartbeat = sin(Time * 4.0 - fLocalPosition.z) + 1.0;
            oColor.rgb = mix(oColor.rgb, vec3(rawTurb + heartbeat, (2.0 - heartbeat) * 0.5, 0.0), pow(coreVal, 0.75));
        }
        //oColor.rgb = 0.00001 * oColor.rgb + vec3(uvs[0].xy, 0.0f);
    }
    
    //float freq = 20.0f;
    //vec3 randomNormal = normalize(vec3(sin(fLocalPosition.x * freq), cos(fLocalPosition.x * freq), sin(fLocalPosition.z * freq) + cos(fLocalPosition.y * freq)));
    //normal = mix(normal, randomNormal, damageTextureAlpha);
    
	oNormal = (normal + 1.0) * 0.5;
    
    oMetallicRoughness.r = metallic;
    oMetallicRoughness.g = roughness;
}