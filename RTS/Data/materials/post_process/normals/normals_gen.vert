// Input
in vec2 vPosition; // Position in screen space

uniform vec4 unUvRect;

// Output
out vec2 fUV;

void main() {
  vec2 subUV = (vPosition + 1.0) / 2.0;
  fUV = unUvRect.xy + unUvRect.zw * subUV;
  gl_Position =  vec4(fUV * 2.0 - 1.0, 0, 1);
}