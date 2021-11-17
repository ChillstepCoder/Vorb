uniform sampler2DArray Atlas;

in vec2 fUV;
flat in float fAtlasPage;

void main()
{
  if (texture(Atlas, vec3(fUV, fAtlasPage)).a < 0.99) {
    discard;
  }
}