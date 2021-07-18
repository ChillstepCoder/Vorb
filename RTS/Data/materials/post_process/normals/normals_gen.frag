uniform sampler2D unAtlasPage;
uniform vec2 unPixelDims;
uniform vec4 unUvRect;

in vec2 fUV;

out vec4 fColor;

const float THRESH = 0.01;
const vec3 COLOR = vec3(0.0, 0.0, 0.0);
const float INTENSITY = 60.0;

float luminance(vec3 pixel) {
	return 0.3 * pixel.r + 0.59 * pixel.g + 0.11 * pixel.b;
}

vec2 clampUV(vec2 UV){
	float x = clamp(UV.x, unUvRect.x + unPixelDims.x * 0.5, unUvRect.x + unUvRect.z - unPixelDims.x * 0.5);
	float y = clamp(UV.y, unUvRect.y + unPixelDims.y * 0.5, unUvRect.y + unUvRect.w - unPixelDims.y * 0.5);
	return vec2(x, y);
}

vec3 getNormalCross() {
	// Get UVS
	vec2 leftUV   = clampUV(vec2(fUV.x - unPixelDims.x, fUV.y));
	vec2 rightUV  = clampUV(vec2(fUV.x - unPixelDims.x, fUV.y));
	vec2 topUV    = clampUV(vec2(fUV.x, fUV.y - unPixelDims.y));
	vec2 bottomUV = clampUV(vec2(fUV.x, fUV.y + unPixelDims.y));

	// Sample neighbors
	vec3 left = texture(unAtlasPage, leftUV).rgb;
	vec3 right = texture(unAtlasPage, rightUV).rgb;
	vec3 top = texture(unAtlasPage, topUV).rgb;
	vec3 bottom = texture(unAtlasPage, bottomUV).rgb;
	
	// Compute luminances
	float leftL = luminance(left);
	float rightL = luminance(right);
	float topL = luminance(top);
	float bottomL = luminance(bottom);
	
	// Luminance offsets
	float dX = rightL - leftL;
    float dY = topL - bottomL;
	
	// Cross product
	vec3 rightVec = normalize(vec3(-unPixelDims.x * INTENSITY, 0.0, dX));
    vec3 frontVec = normalize(vec3(0.0, unPixelDims.y * INTENSITY, dY));
    return (normalize(cross(frontVec, rightVec)) + 1.0) / 2.0;
}

vec3 getNormalSobel() {
	// Get UVS
	vec2 leftUV   = clampUV(vec2(fUV.x - unPixelDims.x, fUV.y));
	vec2 rightUV  = clampUV(vec2(fUV.x - unPixelDims.x, fUV.y));
	vec2 topUV    = clampUV(vec2(fUV.x, fUV.y - unPixelDims.y));
	vec2 bottomUV = clampUV(vec2(fUV.x, fUV.y + unPixelDims.y));

	// Sample neighbors
	vec3 left = texture(unAtlasPage, leftUV).rgb;
	vec3 right = texture(unAtlasPage, rightUV).rgb;
	vec3 top = texture(unAtlasPage, topUV).rgb;
	vec3 bottom = texture(unAtlasPage, bottomUV).rgb;
	
	// Compute luminances
	float leftL = luminance(left);
	float rightL = luminance(right);
	float topL = luminance(top);
	float bottomL = luminance(bottom);
	
	// Luminance offsets
	float dX = leftL - rightL;
    float dY = topL - bottomL;
	
	float xval = ((dX + 1.0) * 0.5);
    float yval = ((dY + 1.0) * 0.5);
    return vec3(xval, yval, 1.0);
}

void main() {
	fColor.rgb = getNormalSobel();
	fColor.a = 1.0;
    
}