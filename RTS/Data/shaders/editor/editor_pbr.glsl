
uniform float unMetallic;
uniform float unRoughness;
uniform float unAmbient;
uniform float unSunIntensity;
uniform float unExposure2;
uniform vec3 unLightDir;
uniform vec3 unCameraPos;
uniform vec3 unSunColor;
layout (binding = 0) uniform samplerCube unIrradianceMap;
layout (binding = 1) uniform samplerCube unPrefilterMap;
layout (binding = 2) uniform sampler2D unBrdfLUT;

//https://github.com/tuxalin/vulkanri
#include "util/pi.glsl"
#include "util/pbr_vulkanri.glsl"

vec3 PBR(vec3 worldPos, vec3 albedo, vec3 normal) {
    // vec2 metallicRoughness = texture(metallicRoughnessMap, inUV).gb;
    const float roughness = max(unRoughness, 0.03); //max(material.roughness * metallicRoughness.x, 0.04);
    const float specular = 1.0; //material.specular;
    const float metallic = unMetallic;//material.metallic * metallicRoughness.y;
    
    const float ao = 1.0;

    const vec3 N = normal; //computeSurfaceNormal(inNormal, worldPos, material.normalStrength, normalMap, inUV);
    const vec3 V = normalize(unCameraPos - worldPos);

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
    const vec3  radiance = unSunColor * unSunIntensity;
    
    const vec3 L = unLightDir;
    const vec3 brdf = cook_torrance_ggx(L, V, N, albedo, metallic, specular, roughness);
    // Outgoing radiance
    const vec3 Lo = brdf * radiance * diffuse_lambert(L, N); 
    

    const float envExposure = unExposure2;
    const float dotNV = max(dot(N, V), 0.0);
    const vec3 kS = f_schlick_roughness(dotNV, albedo, metallic, specular, roughness);
    const vec3 diffuse = ibl_diffuse(N, kS, albedo, metallic, unIrradianceMap, envExposure);
    const vec3 indirectSpecular = ibl_specular(N, V, kS, roughness, unPrefilterMap, envExposure, unBrdfLUT);

    const vec3 ambient = unAmbient * (diffuse + indirectSpecular);
    const vec3 color = (ambient + Lo) * ao;
    //color = 0.0001 * color + vec3(F);
    return color;
}
