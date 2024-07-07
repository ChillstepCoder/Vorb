
#include "AlphaTest.glsl"

in vec3 fNormal;
in vec3 fPosition;

layout (location = 0) out vec4 oColor;

uniform float unAlphaThreshold;
uniform vec4 unColor;

void main() {

    float fresnel = dot(normalize(fNormal), normalize(fPosition));
    fresnel = clamp(fresnel, 0.0, 1.0);

    oColor.rgb = unColor.rgb;
    runAlphaTest(fresnel, unAlphaThreshold);
    
    // AO
    oColor.a = 1.0;
}