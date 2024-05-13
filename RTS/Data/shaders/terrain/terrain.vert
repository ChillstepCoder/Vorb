#include "GlobalUbo.glsl"

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec3 vNormal;

uniform vec3 unPosition;
uniform vec2 unUVRoot;
uniform float unInverseWorldWidth;
uniform float unColorMapScale = 0.005;
uniform float unSnowLevel;
uniform float unPatchWidth;

out float fHeight;
out vec3 fPosition;
out vec2 fUV;
out vec2 fBiomeUV;
out mat3 fTBN;
out float fSnow;
out vec3 fNormal;
out vec3 fViewTangent;
out vec3 fFragPosTangent;
out vec2 fSurfaceUV;

const float TERRAIN_TO_PAD_RATIO = 0.977099236641; // 128/131
const float PAD_OFFSET = 0.00763358778; // 1/131
const float HALF_TEXEL_OFFSET = 0.0038167938931298;

void main() {

    fSurfaceUV = (vPosition.xy / vec2(unPatchWidth)) * TERRAIN_TO_PAD_RATIO + vec2(PAD_OFFSET + HALF_TEXEL_OFFSET);
    
    vec4 worldPos = vPosition + vec4(unPosition - CameraPos, 0.0);
    fBiomeUV = (vPosition.xy + unPosition.xy) * unInverseWorldWidth;
	
	vec3 normal = normalize(vNormal); // Prenormalized on CPU
	vec3 binormal = normalize(cross(normal, vec3(1.0, 0.0, 0.0)));
    vec3 tangent = normalize(cross(binormal, normal));
    
    
    //https://learnopengl.com/Advanced-Lighting/Normal-Mapping
    // re-orthogonalize B with respect to N
    // TODO: Seems to make no difference
    //binormal = normalize(binormal - dot(binormal, normal) * normal);
    
    
	fTBN = mat3(tangent, binormal, normal);
    fNormal = normal;
    
    fSnow = normal.z * unSnowLevel;
    fSnow += clamp((vPosition.z - 50.0) * 0.025 * normal.z, 0.0, 3.0);
    worldPos.z += fSnow * 0.5f;
	
    fUV = unUVRoot + vPosition.xy * unColorMapScale;
    fHeight = vPosition.z;
    fPosition = worldPos.xyz;

    gl_Position = VP * worldPos;
    
    // For displacement, get our world space -> tangent space
    mat3 tfTBN = transpose(fTBN);
    fFragPosTangent = tfTBN * worldPos.xyz;
}