uniform sampler2D FboDepth;

in vec2 fUV;

void main( void ) {
    gl_FragDepth = texture(FboDepth, fUV).r;
}