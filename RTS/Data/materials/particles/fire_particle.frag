
out vec4 fColor;

// http://www.geisswerks.com/ryan/BLOBS/blobs.html
// Using ken perlins funcion for distance: r * r * r * (r * (r * 6 - 15) + 10)              

void main() {
	vec2 position = (gl_PointCoord * 2.0) - 1.0;
    float r = length(position);
	float g = r * r * r * (r * (r * 6.0 - 15.0) + 10.0);
	float attenuatedIntensity = max(1.0 - g, 0.0);
	
	fColor = vec4(1.0, 0.0, 0.0, attenuatedIntensity);
}