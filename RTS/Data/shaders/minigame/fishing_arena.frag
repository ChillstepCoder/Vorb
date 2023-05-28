uniform sampler2D unTexture;

in vec2 fUV;

out vec4 fColor;

const float M_PI = 3.14159265359;
uniform float unSuccessAngle = M_PI * 0.15;
uniform float unFailAngle = M_PI * 0.2;


void main() {
    fColor = texture(unTexture, fUV) * vec4(0.0, 0.0, 0.0, 1.0);
    vec2 offset = fUV * 2.0 - 1.0;
    float length2 = dot(offset, offset);
    if (length2 > 0) {
        float length = sqrt(length2);
        offset = offset / length;
        fColor.r = length * 0.001;
        float angleFromFail = acos(dot(offset, vec2(0.0, -1.0)));
        if (angleFromFail <= unFailAngle) {
            float blendOutMult = pow(1.0 - angleFromFail / unFailAngle, 0.4);
            fColor.r = pow(length, 8.0) * blendOutMult;
        }
        float angleFromSuccess = acos(dot(offset, vec2(0.0, 1.0)));
        if (angleFromSuccess <= unSuccessAngle) {
            float blendOutMult = pow(1.0 - angleFromSuccess / unSuccessAngle, 0.4);
            fColor.g = pow(length, 8.0) * blendOutMult;
        }
    }
    
    
    
    // Don't write 0 alpha (TMP)
    //if (fColor.a <= 0.01) {
    //    discard;
    //}
}