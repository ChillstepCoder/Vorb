// Input
in vec2 vPosition; // Position in screen space

uniform vec4 Rect;
uniform mat4 VP;
uniform vec3 CameraPos;

// Output
out vec2 fUV;

void main() {
  fUV = (vPosition.xy + 1.0) / 2.0;
  vec4 worldPos;
  worldPos = vec4(Rect.xy + fUV * Rect.zw - CameraPos.xy, -CameraPos.z - 1.0, 1.0);
  gl_Position = VP * worldPos;
}