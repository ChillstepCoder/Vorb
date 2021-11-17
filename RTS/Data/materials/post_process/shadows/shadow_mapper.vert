// Input
in vec3 vPosition; // Position in screen space
uniform vec3 CameraPos;

void main() {
  gl_Position =  vec4(vPosition - CameraPos, 1.0);
}