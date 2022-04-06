
// TODO: SHARED
vec3 worldPosFromDepth(float depth, vec2 fboUV) {
    float z = depth * 2.0 - 1.0;

    vec4 clipSpacePosition = vec4(fboUV * 2.0 - 1.0, z, 1.0);
    vec4 viewSpacePosition = InverseP * clipSpacePosition;

    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;

    vec4 worldSpacePosition = InverseV * viewSpacePosition;

    return worldSpacePosition.xyz;
}

float computeDiffuse(vec3 normal, vec3 lightDir) {
  return max(dot(normal, lightDir), 0.0);
}

float computeSpecular(vec3 normal, vec3 lightDir, vec3 position) {

  float shininess = 16.0;
  vec3 viewDir = normalize(position); // Camera is at origin
  vec3 reflectDir = normalize(reflect(lightDir, normal));  
  float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
  return spec;
}

vec3 computePhong(vec3 color, vec3 normal, vec3 lightDir, float ambient, float roughness, float depth, vec2 fboUV, float shadow) {

   
   float lightAmount = 1.0 - shadow;
   // TODO: diffuseAmount
   vec3 position = worldPosFromDepth(depth, fboUV);
   
   float diffuse = computeDiffuse(normal, lightDir) * lightAmount;
   float specular = computeSpecular(normal, lightDir, position) * (1.0 - roughness) * lightAmount;
   return color * (((ambient + (1.0 - ambient) * diffuse) + specular));
}


vec3 computeBlinnPhong(vec3 color, vec3 normal, vec3 lightDir, float ambient, float roughness, float depth, vec2 fboUV, float shadow) {

   float lightAmount = 1.0 - shadow;
   // TODO: diffuseAmount
   vec3 position = worldPosFromDepth(depth, fboUV);
   
   float diffuse = computeDiffuse(normal, lightDir) * lightAmount;
   
   // Specular
   float SPECULAR_HARDNESS = 8.0;
   vec3 H = normalize(lightDir - normalize(position));
   float nDotH = dot(normal, H);
   float intensity = pow(clamp(nDotH, 0.0, 1.0), SPECULAR_HARDNESS);
   float specularPower = 0.6;
   
   float specular = intensity * specularPower * (1.0 - roughness) * lightAmount;
   return color * (((ambient + (1.0 - ambient) * diffuse) + specular));
}
