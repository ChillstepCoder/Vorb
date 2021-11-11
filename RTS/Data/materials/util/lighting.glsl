
float computeDiffuse(vec3 normal, vec3 lightDir) {
  return max(dot(normal, lightDir), 0.0);
}

vec3 computePhong(vec3 color, vec3 normal, vec3 lightDir, float ambient) {
   return color * (ambient + (1.0 - ambient) * computeDiffuse(normal, lightDir));
}