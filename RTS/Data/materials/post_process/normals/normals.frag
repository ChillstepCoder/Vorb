uniform sampler2D Fbo0;
uniform sampler2D FboDepth;
uniform vec2 PixelDims;
uniform float ZoomScale;

in vec2 fUV;

out vec4 fColor;

const float THRESH = 0.01;
const float THICKNESS_PX = 0.01;
const vec3 COLOR = vec3(0.0, 0.0, 0.0);
const float INTENSITY = 60.0;

float luminance(vec3 pixel) {
	return 0.3 * pixel.r + 0.59 * pixel.g + 0.11 * pixel.b;
}

void main() {
    vec3 center = texture(Fbo0, fUV).rgb;
	
	float xWidth = PixelDims.x * THICKNESS_PX * ZoomScale;
	float yWidth = PixelDims.y * THICKNESS_PX * ZoomScale;
	
	// Sample neighbors
	vec3 left = texture(Fbo0, vec2(fUV.x - PixelDims.x * THICKNESS_PX * ZoomScale, fUV.y)).rgb;
	vec3 right = texture(Fbo0, vec2(fUV.x - PixelDims.x * THICKNESS_PX * ZoomScale, fUV.y)).rgb;
	vec3 top = texture(Fbo0, vec2(fUV.x, fUV.y - PixelDims.y * THICKNESS_PX * ZoomScale)).rgb;
	vec3 bottom = texture(Fbo0, vec2(fUV.x, fUV.y + PixelDims.y * THICKNESS_PX * ZoomScale)).rgb;
	
	float depthTop = texture(FboDepth, vec2(fUV.x, fUV.y - PixelDims.y * THICKNESS_PX * ZoomScale)).r;
	float depthBottom = texture(FboDepth, vec2(fUV.x, fUV.y + PixelDims.y * THICKNESS_PX * ZoomScale)).r;
	
	// Compute luminances
	float centerL = luminance(center);
	float leftL = luminance(left);
	float rightL = luminance(right);
	float topL = luminance(top);
	float bottomL = luminance(bottom);
	
	float dX = rightL - leftL;
    float dY = topL - bottomL + (depthTop - depthBottom) * 30.0;
    
    vec3 rightVec = normalize(vec3(-xWidth * INTENSITY, 0.0, dX));
    vec3 frontVec = normalize(vec3(0.0, yWidth * INTENSITY, dY));
    fColor.rgb = (normalize(cross(frontVec, rightVec)) + 1.0) / 2.0;
	
	//float xDeg = (centerL - leftL + rightL) * 1.5 + 0.5;
	//float yDeg = (centerL - topL + bottomL) * 1.5 + 0.5;
	//fColor.rgb = vec3(xDeg, yDeg, 0.5);
	fColor.a = 1.0;
    
}