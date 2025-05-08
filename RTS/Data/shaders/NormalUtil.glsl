vec3 reconstructNormal(vec2 normalXY) {
    float den = max(1.0 - (normalXY.x * normalXY.x) - (normalXY.y * normalXY.y), 0.001);
    return vec3(normalXY, sqrt(den));
}

vec3 rotateOffsetToNormal(vec3 offset, vec3 normal) {
    vec3 up = vec3(0.0, 0.0, 1.0);
    vec3 right = cross(up, normal);

    if(length(right) < 0.0001) {
        return offset;
    }

    right = normalize(right); // TODO: unneeded?

    vec3 front = cross(right, normal);
    return offset.x * right + offset.y * front + offset.z * normal;
}