
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

  float shininess = 32.0;
  vec3 viewDir = normalize(position); // Camera is at origin
  vec3 reflectDir = reflect(lightDir, normal);  
  float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
  return spec;
}

vec3 computePhong(vec3 color, vec3 normal, vec3 lightDir, float ambient, float roughness, float depth, vec2 fboUV) {
   vec3 position = worldPosFromDepth(depth, fboUV);
   float diffuse = computeDiffuse(normal, lightDir);
   float specular = computeSpecular(normal, lightDir, position) * (1.0 - roughness);
   return color * (((ambient + (1.0 - ambient) * diffuse) + specular));
}
