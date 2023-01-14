
uniform int unRenderMode = 0;

const int RENDER_MODE_LIT = 0;
const int RENDER_MODE_UNLIT = 1;
const int RENDER_MODE_NORMALS = 2;
const int RENDER_MODE_UVS = 3;
const int RENDER_MODE_BLEND_TEST = 4;
const int RENDER_MODE_EDGE_TEST = 5;
const int RENDER_MODE_WIREFRAME = 6;

vec4 getEditorOutputPixelColor(vec3 pixelColor, vec3 normal, vec3 worldPos, vec2 screenUV) {
    vec4 rv;
    if (unRenderMode == RENDER_MODE_LIT) {
        vec3 sunPosition = (unVP * vec4(SunPosition, 0.0)).rgb;
        rv.rgb = lightPixel(pixelColor, normal, worldPos, screenUV, 0.2, 0.0, 0.0, inverse(unVP), SunColor, SunHeight, SunPosition);
    } else if (unRenderMode == RENDER_MODE_NORMALS) {
        rv.rgb = (normal + 1.0) * 0.5;
    } else if (unRenderMode == RENDER_MODE_UVS) {
        rv.rg = fUV;
        rv.b = 0.0;
    } else {
        // Unlit
        rv.rgb = pixelColor;
    }
    rv.a = 1.0;
    return rv;
}