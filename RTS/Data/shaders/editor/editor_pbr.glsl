
uniform float unMetallic;
uniform float unRoughness;
uniform float unAmbient;
uniform float unSunIntensity;
uniform vec3 unLightDir;
uniform vec3 unCameraPos;
uniform vec3 unSunColor;
uniform samplerCube unIrradianceMap;
uniform samplerCube unPrefilterMap;
uniform sampler2D unBrdfLUT;

const float PI = 3.14159265359;

//#include "editor/editor_util.glsl"

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 calculateAmbient(vec3 albedo, vec3 normal, vec3 viewNormal, vec3 f0, float roughness) {
    vec3 kS = fresnelSchlickRoughness(max(dot(normal, viewNormal), 0.0), f0, roughness); 
    vec3 kD = 1.0 - kS;
    vec3 irradiance = texture(unIrradianceMap, normal).rgb;
    vec3 diffuse    = irradiance * albedo;
    return (kD * diffuse); 
}

vec3 PBRLearnOpengl(vec3 worldPos, vec3 albedo, vec3 normal) {
    vec3  lightColor  = unSunColor * unSunIntensity;
    float cosTheta    = max(dot(normal, unLightDir), 0.0);
    vec3  radiance    = lightColor * cosTheta;
    
    vec3 viewNormal = normalize(unCameraPos - worldPos);
    vec3 halfVector = normalize(viewNormal + unLightDir);
    
    vec3 f0 = vec3(0.04); // Surface reflection at zero incidence
    f0 = mix(f0, albedo, unMetallic);
    
    float NDF = DistributionGGX(normal, halfVector, unRoughness);       
    float G  = GeometrySmith(normal, viewNormal, unLightDir, unRoughness);     
    vec3 F = FresnelSchlickRoughness(max(dot(halfVector, viewNormal), 0.0), f0, unRoughness);

    vec3 kSpecular = F;
    vec3 kDiffuse = vec3(1.0) - kSpecular;
    kDiffuse *= 1.0 - unMetallic;

    // Cook-Torrance BRDF for sunlight
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewNormal), 0.0) * max(dot(normal, unLightDir), 0.0) + 0.0001;
    vec3 specular     = numerator / denominator;  


    
    // Calculate reflectance
    float NdotL = max(dot(normal, unLightDir), 0.0);        
    vec3 outgoingRadiance = (kDiffuse * albedo / PI + specular) * radiance * NdotL;
    
    vec3 ambient = calculateAmbient(albedo, normal, viewNormal, f0, unRoughness) * unAmbient;
    vec3 color   = ambient + outgoingRadiance;
    //color = 0.0001 * color + vec3(F);
    return color;
}
