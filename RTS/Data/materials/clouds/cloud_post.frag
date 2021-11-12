uniform sampler2DArray Atlas;
uniform sampler2D CloudFbo;
uniform vec3 SunPositionCameraRelative;
uniform vec2 unPixelDims;
uniform float unAmbient;

uniform vec4 unCloudTextureRect;
uniform float unCloudTexturePage;

#include "../util/gaussian_blur.glsl"
#include "../util/lighting.glsl"

in vec2 fUV;

layout (location = 0) out vec4 fColor;
layout (location = 1) out vec4 fNormal;

void main() {
    // fColor = texture(CloudFbo, fUV);
	float baseAlpha = texture2D(CloudFbo, fUV).a;
	//vec3 norm = normalize(blur13noalpha(CloudFbo, fUV, unPixelDims, vec2(baseAlpha * 3.0, 0.0)));
	vec3 norm = normalize(texture2D(CloudFbo, fUV).rgb);
	
	float z = step(0.000001, norm.z);
	if (z == 0.0) {
	  discard;
	}
	
	fNormal = vec4((norm + 1.0) * 0.5, 1.0);
	vec2 tex = ((norm.xy + vec2(1.0)) * 0.5);
	tex.y = 1.0 - tex.y;
	fColor.rgba = texture(Atlas, vec3(unCloudTextureRect.xy + tex * unCloudTextureRect.zw, unCloudTexturePage)).rgba;
	// Fake scattering
	vec3 frontRGB = computePhong(fColor.rgb, norm, SunPositionCameraRelative, unAmbient);
	vec3 backRGB = computePhong(fColor.rgb, vec3(norm.x, norm.y, -norm.z), SunPositionCameraRelative, unAmbient);
	fColor.rgb = frontRGB * 0.7 + backRGB * 0.3;
}