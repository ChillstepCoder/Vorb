uniform sampler2DArray Atlas;
uniform sampler2D Fbo0;
uniform sampler2D FboNormals;
uniform sampler2D FboDepth;
uniform sampler2D FboRoughness;
uniform sampler2D ShadowTexture;
uniform sampler2D SSAOTexture;
uniform vec4 GradientRect;
uniform float GradientAtlasPage;
uniform vec3 ShadowColor;
uniform vec3 SSAOColor;
uniform float unGamma;
uniform float unExposure;
uniform float unHazeExponent;
uniform int unTonemapOperator;
uniform int unLightingModel;


in vec2 fUV;
out vec4 fColor;

#include "GlobalUbo.glsl"
#include "scene_lighting.glsl"


void main() {

	float depth = texture(FboDepth, fUV).r;
	float isSky = step(0.999999999, depth);
	float shadow = texture(ShadowTexture, fUV).r;
    vec3 worldPos = worldPosFromDepth(depth, fUV);
	vec3 fboColor = texture(Fbo0, fUV).rgb;
    vec3 normal = texture(FboNormals, fUV).rgb;
	float roughness = texture(FboRoughness, fUV).r;
	roughness = max(roughness, isSky);
	normal = normal * 2.0 - 1.0;
    
	fColor.rgb = lightPixel(fboColor, normal, worldPos, fUV, roughness, isSky, shadow);
	fColor.a = 1.0;
	
}