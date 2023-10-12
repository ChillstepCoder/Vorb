#include "MaterialData.glsl"
#include "BillboardSSBO.glsl"
#include "GlobalUbo.glsl"

uniform vec4 unSphereNormalRect;
uniform float unSphereNormalPage;

in vec2 fUV;
flat in uint fMaterialIndex;
in vec4 fTint;
in mat3 fTBN;

layout (location = 0) out vec4 fNormal;

void main() {
    MaterialData mtl = inMaterials[fMaterialIndex];
	vec3 norm;
    if (mtl.normalMap > 0) {
        norm = sampleMaterialNormal(mtl, fUV);
    } else {
        norm = vec3(0.0, 0.0, 1.0);
    }
    // Don't write 0 alpha (TMP?)
	// TODO: Noise on this edge so that its fuzzy average
    
    // Replace alpha (Using GRAYA, so alpha is in G)
    fNormal.a = sampleMaterialAlbedo(mtl, fUV).g;
    
    // TODO: If we use the cloud UV instead of screen UV for this, we can
    // layer transparency in a better way
    tryDiscardTransparentPixel(fNormal.a);
    
    // TODO: Try uncommenting this line, see if talia likes it
    //fNormal.a = 1.0;

    // https://gamedev.stackexchange.com/questions/16588/computing-gl-fragdepth
    float ndcDepth = (2.0 * gl_FragCoord.z - gl_DepthRange.near - gl_DepthRange.far) / (gl_DepthRange.diff);
	float clipDepth = ndcDepth / gl_FragCoord.w;
	vec4 cameraSpacePosition = InverseP * vec4(0.0, 0.0, clipDepth, 1.0 / gl_FragCoord.w);
    
    // SUUPER HACKY DEPTH BULLSHIT LOL
	cameraSpacePosition.z += norm.z * 10.0 - step(0.01, (1.0 - fNormal.a)) * 1000.0;
    
    // TODO: Do we actually care about this? gl_FragDepth is expensive
    vec4 clipPos = P * vec4(cameraSpacePosition.xyz, 1.0);
    ndcDepth = clipPos.z / clipPos.w;
    gl_FragDepth = ((gl_DepthRange.diff * ndcDepth) + gl_DepthRange.near + gl_DepthRange.far) / 2.0;
	
    // Screen space
	norm = norm * 2.0 - 1.0;
    // TODO: THIS IS HACK FOR NONSTANDARD, REMOVE WHEN NORMALS ARE GENERATED
    norm = normalize(norm);
    norm.y = -norm.y;
    
    norm = fTBN * norm;
    
    fNormal.rgb = (norm + 1.0) * 0.5;
    
    //fNormal.a = 1.0;
}