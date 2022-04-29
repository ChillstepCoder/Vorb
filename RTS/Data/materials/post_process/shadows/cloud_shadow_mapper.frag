
in vec2 fUV;
flat in int fTextureIndex;

out vec4 fColor;

void main()
{
  //if (texture(Atlas, vec3(fUV, fAtlasPage)).a < 0.99) {
  //  discard;
  //}
  float depth = gl_FragCoord.z;
  
  // Partial derivatives of depth
  // https://www.youtube.com/watch?v=F5QAkUloGOs
  // Check video description for explanation
  float dx = dFdx(depth);
  float dy = dFdy(depth);
  float moment2 = depth * depth + 0.25 * (dx * dx + dy * dy);
  
  fColor = vec4(depth, moment2, 0.0, 1.0);
}