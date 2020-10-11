uniform sampler2D Fbo0;
uniform sampler2D FboDepth;
uniform vec2 PixelDims;
uniform float ZoomScale;

in vec2 fUV;

out vec4 fColor;

const float THRESH = 0.01;
const float THICKNESS_PX = 0.05;
const vec3 COLOR = vec3(0.0, 0.0, 0.0);

void main() {

	fColor = texture(Fbo0, fUV);
    float z = texture(FboDepth, fUV).r;           // fetch the z-value from our depth texture
    
	float rightZ = texture(FboDepth, vec2(fUV.x + PixelDims.x * THICKNESS_PX * ZoomScale, fUV.y)).r;
	float topZ = texture(FboDepth, vec2(fUV.x, fUV.y - PixelDims.y * THICKNESS_PX * ZoomScale)).r;
	if (abs(rightZ - z) > THRESH || abs(topZ - z) > THRESH) {
		fColor.rgb = COLOR;
	}
}