uniform sampler2DArray Atlas;
uniform vec4 unMaskRect;
uniform float unMaskPage; 

in vec3 fVelocity;

out vec4 fColor;

// http://www.geisswerks.com/ryan/BLOBS/blobs.html
// Using ken perlins funcion for distance: r * r * r * (r * (r * 6 - 15) + 10)         

const float VELOCITY_WARP_MAX = 0.3;

float getIntensity() {
	vec2 position = (gl_PointCoord * 2.0) - 1.0;
    //vec2 vel = abs(fVelocity.xy * 100.0);
	//position *= 1.0 - min(vel, VELOCITY_WARP_MAX);
	
    float r = length(position);
	r = min(r, 1.0);
	float g = r * r * r * (r * (r * 6.0 - 15.0) + 10.0);
	return 1.0 - g;
}

float getIntensityTextured() {
    //vec2 position = (gl_PointCoord * 2.0) - 1.0;
    //vec2 vel = abs(fVelocity.xy * 100.0);
	//position *= 1.0 - min(vel, VELOCITY_WARP_MAX);
	//vec2 texCoord = (position + 1.0) * 0.5;

    return texture(Atlas, vec3(unMaskRect.xy + gl_PointCoord * unMaskRect.zw, unMaskPage)).r;
}

void main() {
	fColor = vec4(1.0, 0.0, 0.0, getIntensityTextured());
}