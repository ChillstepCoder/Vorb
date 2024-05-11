#include "GlobalUbo.glsl"

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in float vRoadIntensity;
layout(location = 3) in uint vRoadMaterialIndex;

uniform vec3 unPosition;
uniform vec2 unUVRoot;
uniform float unInverseWorldWidth;
uniform float unColorMapScale = 0.005;
uniform float unSnowLevel;

out float fHeight;
out float fRoadIntensity;
flat out uint fRoadMaterialIndex;
out vec3 fPosition;
out vec2 fUV;
out vec2 fBiomeUV;
out mat3 fTBN;
out float fSnow;
out vec3 fNormal;
out vec3 fViewTangent;
out vec3 fFragPosTangent;

void main() {
    vec4 vertexPos = vPosition;
    vec4 worldPos = vertexPos + vec4(unPosition - CameraPos, 0.0);
    fBiomeUV = (vertexPos.xy + unPosition.xy) * unInverseWorldWidth;
    fRoadIntensity = vRoadIntensity;
    fRoadMaterialIndex = vRoadMaterialIndex;
	
	vec3 normal = vNormal; // Prenormalized on CPU
	vec3 binormal = normalize(cross(normal, vec3(1.0, 0.0, 0.0)));
    vec3 tangent = normalize(cross(binormal, normal));
    
    
    //https://learnopengl.com/Advanced-Lighting/Normal-Mapping
    // re-orthogonalize B with respect to N
    // TODO: Seems to make no difference
    //binormal = normalize(binormal - dot(binormal, normal) * normal);
    
    
	fTBN = mat3(tangent, binormal, normal);
    fNormal = normal;
    
    fSnow = normal.z * unSnowLevel;
    fSnow += clamp((vertexPos.z - 50.0) * 0.025 * normal.z, 0.0, 3.0);
    worldPos.z += fSnow * 0.5f;
	
    fUV = unUVRoot + vPosition.xy * unColorMapScale;
    fHeight = vertexPos.z;
    fPosition = worldPos.xyz;

    gl_Position = VP * worldPos;
    
    // For displacement, get our world space -> tangent space
    mat3 tfTBN = transpose(fTBN);
    fFragPosTangent = tfTBN * worldPos.xyz;
}