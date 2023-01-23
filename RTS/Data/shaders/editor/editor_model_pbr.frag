#include "MaterialData.glsl"
#include "GlobalUbo.glsl"
#include "lighting/scene_lighting.glsl"

uniform mat4 unVP;
uniform float unMetallic;
uniform float unRoughness;
uniform vec3 unLightDir;
uniform vec3 unLightColor = vec3(23.47, 21.31, 20.79);
uniform vec3 unCameraPos;

in vec2 fUV;
in vec3 fWorldPos;
in vec2 fScreenPos;
flat in uint fMaterialIndex;
in vec4 fTint;
in mat3 fTBN;

layout (location = 0) out vec4 oColor;
layout (location = 1) out vec3 oNormal;


const float PI = 3.14159265359;

#include "editor/editor_util.glsl"

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

vec3 PBRLearnOpengl(vec3 worldPos, vec3 albedo, vec3 normal) {
    vec3  lightColor  = vec3(23.47, 21.31, 20.79);
    float cosTheta    = max(dot(normal, unLightDir), 0.0);
    vec3  radiance    = lightColor * cosTheta;
    
    vec3 viewNormal = normalize(unCameraPos - worldPos);
    vec3 halfVector = normalize(viewNormal + unLightDir);
    
    vec3 f0 = vec3(0.04); // Surface reflection at zero incidence
    f0 = mix(f0, albedo, unMetallic);
    
    vec3 F = fresnelSchlick(max(dot(halfVector, viewNormal), 0.0), f0);
    float NDF = DistributionGGX(normal, halfVector, unRoughness);       
    float G  = GeometrySmith(normal, viewNormal, unLightDir, unRoughness);     

    // Cook-Torrance BRDF
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewNormal), 0.0) * max(dot(normal, unLightDir), 0.0) + 0.0001;
    vec3 specular     = numerator / denominator;  
    
    vec3 kSpecular = F;
    vec3 kDiffuse = vec3(1.0) - unMetallic;
    
    // Calculate reflectance
    float NdotL = max(dot(normal, unLightDir), 0.0);        
    vec3 outgoingRadiance = (kDiffuse * albedo / PI + specular) * radiance * NdotL;
    
    vec3 ambient = vec3(0.03) * albedo;
    vec3 color   = ambient + outgoingRadiance;
    return color;
}

void main() {

    vec3 normal;
    vec4 color;
    getMaterialPixelInfo(fMaterialIndex, fUV, color, normal, fTint);
    
    tryDiscardTransparentPixel(color.a);
	
	// Normal to tangent space
    normal = normalize(fTBN * normal);
    
    oColor.rgb = PBRLearnOpengl(fWorldPos, color.rgb, normal);
    oColor.a = 1.0;
    //oColor = getEditorOutputPixelColor(color.rgb, normal, fWorldPos, fScreenPos);
    oNormal = (normal + 1.0) * 0.5;
}