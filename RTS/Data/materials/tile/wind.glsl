// Created by inigo quilez - iq/2013
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.

// Value Noise (http://en.wikipedia.org/wiki/Value_noise), not to be confused with Perlin's
// Noise, is probably the simplest way to generate noise (a random smooth signal with 
// mostly all its energy in the low frequencies) suitable for procedural texturing/shading,
// modeling and animation.
//
// It produces lowe quality noise than Gradient Noise (https://www.shadertoy.com/view/XdXGW8)
// but it is slightly faster to compute. When used in a fractal construction, the blockyness
// of Value Noise gets qcuikly hidden, making it a very popular alternative to Gradient Noise.
//
// The princpiple is to create a virtual grid/latice all over the plane, and assign one
// random value to every vertex in the grid. When querying/requesting a noise value at
// an arbitrary point in the plane, the grid cell in which the query is performed is
// determined (line 30), the four vertices of the grid are determined and their random
// value fetched (lines 35 to 38) and then bilinearly interpolated (lines 35 to 38 again)
// with a smooth interpolant (line 31 and 33).


// Value    Noise 2D: https://www.shadertoy.com/view/lsf3WH
// Value    Noise 3D: https://www.shadertoy.com/view/4sfGzS
// Gradient Noise 2D: https://www.shadertoy.com/view/XdXGW8
// Gradient Noise 3D: https://www.shadertoy.com/view/Xsl3Dl
// Simplex  Noise 2D: https://www.shadertoy.com/view/Msf3WH


float hash(vec2 p)  // replace this by something better
{
    p  = 50.0*fract( p*0.3183099 + vec2(0.71,0.113));
    return -1.0+2.0*fract( p.x*p.y*(p.x+p.y) );
}

float noise( in vec2 p )
{
    vec2 i = floor( p );
    vec2 f = fract( p );
	
	vec2 u = f*f*(3.0-2.0*f);

    return mix( mix( hash( i + vec2(0.0,0.0) ), 
                     hash( i + vec2(1.0,0.0) ), u.x),
                mix( hash( i + vec2(0.0,1.0) ), 
                     hash( i + vec2(1.0,1.0) ), u.x), u.y);
}

float fbm( in vec2 p)
{
    mat2 m = mat2( 1.6,  1.2, -1.2,  1.6 );
    float f = 0.0;
    f += 0.5000*noise( p ); p = m*p;
    f += 0.2500*noise( p ); p = m*p;
    f += 0.1250*noise( p ); p = m*p;
    f += 0.0625*noise( p );
    return f;
}

float norm( in float f )
{
    return 0.5 + 0.5 * f;
}


const float AMPLITUDE = 0.3;

float getWindAtPosition(in float Time, in vec4 worldPos) {
    // Standard wind forces
    float windForce = fbm(vec2(worldPos.x + Time * 0.2, worldPos.y)) * AMPLITUDE;
    // Rolling wind
    windForce += sin((worldPos.x - worldPos.y) * 0.2 + Time * 0.5) * 0.2;
    return windForce * worldPos.z;
}

// ***********************************************************************************