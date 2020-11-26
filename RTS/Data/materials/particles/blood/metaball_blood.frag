uniform sampler2D ParticleFbo;
uniform vec2 unPixelDims;

in vec2 fUV;

layout (location = 0) out vec4 fColor;
layout (location = 1) out vec4 fNormal;

const float THRESHOLD = 0.25;
const float NORMAL_INTENSITY = 4.0;

vec3 getNormalSobel() {
	// Get UVS
	vec2 leftUV   = vec2(fUV.x - unPixelDims.x, fUV.y);
	vec2 rightUV  = vec2(fUV.x - unPixelDims.x, fUV.y);
	vec2 topUV    = vec2(fUV.x, fUV.y - unPixelDims.y);
	vec2 bottomUV = vec2(fUV.x, fUV.y + unPixelDims.y);

	// Sample neighbors
	float left = texture(ParticleFbo, leftUV).r;
	float right = texture(ParticleFbo, rightUV).r;
	float top = texture(ParticleFbo, topUV).r;
	float bottom = texture(ParticleFbo, bottomUV).r;

	float dX = (right - left) * NORMAL_INTENSITY;
    float dY = (bottom - top) * NORMAL_INTENSITY;
	
	float xval = ((dX + 1.0) * 0.5);
    float yval = ((dY + 1.0) * 0.5);
    return vec3(xval, yval, 1.0);
}

void main() {
	float intensity = texture(ParticleFbo, fUV).r;
	intensity = step(THRESHOLD, intensity);
	
    fColor = vec4(intensity, 0.0, 0.0, intensity); 
	
	fNormal.rgb = getNormalSobel();
	fNormal.a = intensity;
	// Darken
	fColor.r *= fNormal.g;
}