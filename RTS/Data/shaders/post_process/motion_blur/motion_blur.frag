uniform sampler2D Fbo0;
uniform sampler2D PrevFbo0;
uniform vec2 PixelDims;
uniform float ZoomScale;

in vec2 fUV;

out vec4 fColor;

const float THRESH = 0.01;
const float THICKNESS_PX = 0.05;
const vec3 COLOR = vec3(0.0, 0.0, 0.0);

void main() {
	fColor.rgb = texture(Fbo0, fUV).rgb * 0.5 + texture(PrevFbo0, fUV).rgb * 0.5;
	fColor.a = 1.0;
    
}