// Input
in vec4 vPosition; // Position in world space
//in vec3 vVelocity;
in float vSize;
//in float vLifetime;

uniform mat4 VP;
uniform float ZoomScale;
uniform float MaxLifetime;
uniform float BlendFade;
const float PARTICLE_SCALE_RATIO = 1.0 / 1024.0;

//out vec3 fVelocity;
//out float fLifetime;

void main() {
  //fVelocity = vVelocity;
  //fLifetime = vLifetime;
  
  gl_PointSize = vSize * PARTICLE_SCALE_RATIO * ZoomScale;
  
  vec4 position = vPosition;
  position.y += position.z;
  //position.z += 0.01;
  gl_Position = VP * position;
}