
// Computes TBN and adjusts normal + tangent
mat3 computeTbn(mat3 modelMatrix3, inout vec3 normal, inout vec3 tangent) {
    normal = modelMatrix3 * normal;
    tangent = modelMatrix3 * tangent;
    
	vec3 bitangent = cross(normal, tangent);
	return mat3(tangent, bitangent, normal);
}