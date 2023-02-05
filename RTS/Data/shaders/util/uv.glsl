vec2 unpackUV(vec2 uvsPacked) {
    return uvsPacked * 8.0; // Scale to the hard coded c++ UV_RANGE
}