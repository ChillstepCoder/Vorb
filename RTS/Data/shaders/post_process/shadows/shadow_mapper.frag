
out vec2 oColor;

void main()
{
  float depth = gl_FragCoord.z;
  
  // Partial derivatives of depth
  // https://www.youtube.com/watch?v=F5QAkUloGOs
  // Check video description for explanation
  float dx = dFdx(depth);
  float dy = dFdy(depth);
  float moment2 = depth * depth + 0.25 * (dx * dx + dy * dy);
   
  // TODO: RG16
  // Variance shadow mapping
  oColor = vec2(depth,  moment2);
}