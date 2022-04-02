uniform sampler2DArray Atlas;
#include "../GlobalUbo.glsl"

uniform vec4 unSphereNormalRect;
uniform float unSphereNormalPage;

in vec2 fUV;
in vec2 fPosition;
flat in float fAtlasPage;
in vec4 fTint;
in mat3 fTBN;

layout (location = 0) out vec4 fNormal;

void main() {
    fNormal.a = texture(Atlas, vec3(fUV, fAtlasPage)).a * fTint.a;
	vec3 norm = texture(Atlas, vec3(unSphereNormalRect.xy + fPosition * unSphereNormalRect.zw, unSphereNormalPage)).rgb;
	norm = norm * 2.0 - 1.0;
    // Don't write 0 alpha (TMP?)
	// TODO: Noise on this edge so that its fuzzy average

    // https://gamedev.stackexchange.com/questions/16588/computing-gl-fragdepth
    float ndcDepth = (2.0 * gl_FragCoord.z - gl_DepthRange.near - gl_DepthRange.far) / (gl_DepthRange.diff);
	float clipDepth = ndcDepth / gl_FragCoord.w;
	vec4 cameraSpacePosition = InverseP * vec4(0.0, 0.0, clipDepth, 1.0 / gl_FragCoord.w);
	cameraSpacePosition.z += norm.z * 10.0;
    vec4 clipPos = P * vec4(cameraSpacePosition.xyz, 1.0);
    ndcDepth = clipPos.z / clipPos.w;
    gl_FragDepth = ((gl_DepthRange.diff * ndcDepth) + gl_DepthRange.near + gl_DepthRange.far) / 2.0;
	
    // Screen space
    norm = fTBN * norm;
    
    // Adjust normals to be more severe
	fNormal.rgb = (normalize(vec3(norm.x, norm.y, norm.z * 0.3)) + 1.0) * 0.5;
    
    if (fNormal.a < 0.99) {
        discard;
    }
    
    float centerDistance = length(fPosition - vec2(0.5)) * 1.0;
    
	fNormal.a = 1.0;
}