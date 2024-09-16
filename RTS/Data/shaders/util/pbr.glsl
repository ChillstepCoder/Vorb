
uniform samplerCube unIrradianceMap;
uniform samplerCube unPrefilterMap;
uniform sampler2D unBrdfLUT;
uniform vec3 ShadowColor;

//https://github.com/tuxalin/vulkanri
#include "util/pi.glsl"
#include "util/pbr_vulkanri.glsl"

vec3 PBR(vec3 worldPos, vec3 albedo, vec3 normal, float metallic, float roughness, float ao, float shadow, float ambient, float exposure, vec3 sunColor, vec3 sunDir, vec3 cameraPos) {
    
    const float SHADOW_MULT = 0.6;
    float sunIntensity = max(SunHeight, 0.0);
    float shadowMult = shadow * sunIntensity * SHADOW_MULT;
    
    // vec2 metallicRoughness = texture(metallicRoughnessMap, inUV).gb;
    roughness = max(roughness, 0.03); //max(material.roughness * metallicRoughness.x, 0.04);
    const float specular = 1.0; //material.specular;
    //const float metallic = unMetallic;//material.metallic * metallicRoughness.y;
    
    const vec3 N = normal; //computeSurfaceNormal(inNormal, worldPos, material.normalStrength, normalMap, inUV);
    const vec3 V = normalize(cameraPos - worldPos);

//    vec3 Lo = vec3(0.0);
//    for (int i = 0; i < lightParams.lights.length(); i++) 
//    {
//        vec3 lightPos = lightParams.lights[i].xyz;
//        vec3 L = lightPos - worldPos;
//
//        // light radiance
//        float distance = length(L);
//        float attenuation = clamp(lightAttenuation / (1.0 + distance * distance), 0.0, 1.0);
//        float lightIntensity = lightParams.lights[i].a;
//        vec3 radiance = lightColor * lightIntensity * attenuation;
//
//        // diffuse + specular BRDF
//        L /= distance;
//        vec3 brdf = cook_torrance_ggx(L, V, N, albedo, metallic, specular, roughness);
//        // add to total outgoing radiance
//        Lo += brdf * radiance * diffuse_lambert(L, N); 
//    }
    
    // Sunlight
    const vec3 radiance = sunColor;
    
    const vec3 L = sunDir;
    const vec3 brdf = cook_torrance_ggx(L, V, N, albedo, metallic, specular, roughness);
    // Outgoing radiance
    const vec3 Lo = brdf * radiance * diffuse_lambert(L, N) * (1.0 - shadowMult); 
    

    const float dotNV = max(dot(N, V), 0.0);
    const vec3 kS = f_schlick_roughness(dotNV, albedo, metallic, specular, roughness);
    const vec3 diffuse = ibl_diffuse(N, kS, albedo, metallic, unIrradianceMap, exposure);
    const vec3 indirectSpecular = ibl_specular(N, V, kS, roughness, unPrefilterMap, exposure, unBrdfLUT);

    const vec3 ambientV = ambient * (diffuse + indirectSpecular);
    vec3 color = (ambientV + Lo) * ao;
    
    // Shadows
	color = mix(color, color * ShadowColor, shadowMult);
    return color;
}
