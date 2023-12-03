#include "MaterialData.glsl"
#include "BillboardSSBO.glsl"

in vec2 fUV;
flat in uint fMaterialIndex;

out vec4 fColor;

void main()
{
  MaterialData mtl = inMaterials[fMaterialIndex];
  // TODO: There is something wrong with the dds compression for cloud_sil as its storing alpha in G...
  if (sampleMaterialAlbedo(mtl, fUV).g < 0.99) {
    discard;
  }
  float depth = gl_FragCoord.z;
  
  // Partial derivatives of depth
  // https://www.youtube.com/watch?v=F5QAkUloGOs
  // Check video description for explanation
  float dx = dFdx(depth);
  float dy = dFdy(depth);
  float moment2 = depth * depth + 0.25 * (dx * dx + dy * dy);
  
  fColor = vec4(depth, moment2, 0.0, 1.0);
}