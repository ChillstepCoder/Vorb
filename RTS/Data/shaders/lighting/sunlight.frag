uniform sampler2D unFboDiffuse;
uniform sampler2D unFboNormals;
uniform sampler2D unFboDepth;
uniform sampler2D unShadowTexture;

in vec2 fUV;
out vec4 fColor;

#include "GlobalUbo.glsl"
#include "scene_lighting.glsl"
#include "util/depth.glsl"


void main() {

	float depth = texture(unFboDepth, fUV).r;
    vec3 worldPos = worldPosFromDepth(depth, fUV, InverseV, InverseP);

	float isSky = step(0.999999999, depth);
	float shadow = texture(unShadowTexture, fUV).r;
	vec3 fboDiffuse = texture(unFboDiffuse, fUV).rgb;
    vec3 fboNormal = texture(unFboNormals, fUV).rgb;
	float roughness = 0.25;
	roughness = max(roughness, isSky);
	fboNormal = fboNormal * 2.0 - 1.0;
    
	fColor.rgb = lightPixel(fboDiffuse, fboNormal, worldPos, fUV, roughness, isSky, shadow * 0.5); // TODO: NOTE THIS 0.5 IS HARD CODED!
	fColor.a = 1.0;
	
}