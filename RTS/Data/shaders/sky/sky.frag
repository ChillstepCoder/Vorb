// TODO: Indoor mask
uniform sampler2D StarfieldTexture;
uniform sampler2D SkyboxLeft;
uniform sampler2D SkyboxRight;
uniform sampler2D SkyboxFront;
uniform sampler2D SkyboxBack;
uniform sampler2D SkyboxUp;

#include "../GlobalUbo.glsl"

in vec2 fUV;
in vec3 fPosition;
in vec3 fSkyVector;

out vec3 oColor;

// https://en.wikipedia.org/wiki/Cube_mapping
void convert_xyz_to_cube_uv(float x, float y, float z, inout int index, inout vec2 uv) {
  float absX = abs(x);
  float absY = abs(y);
  float absZ = abs(z);
  
  bool isXPositive = x >= 0 ? true : false;
  bool isYPositive = y >= 0 ? true : false;
  bool isZPositive = z >= 0 ? true : false;
  
  float maxAxis, uc, vc;
  
  // POSITIVE X
  if (isXPositive && absX >= absY && absX >= absZ) {
    // u (0 to 1) goes from +z to -z
    // v (0 to 1) goes from -y to +y
    maxAxis = absX;
    uc = -z;
    vc = y;
    index = 0;
  }
  // NEGATIVE X
  else if (!isXPositive && absX >= absY && absX >= absZ) {
    // u (0 to 1) goes from -z to +z
    // v (0 to 1) goes from -y to +y
    maxAxis = absX;
    uc = z;
    vc = y;
    index = 1;
  }
  // POSITIVE Y
  else if (isYPositive && absY >= absX && absY >= absZ) {
    // u (0 to 1) goes from -x to +x
    // v (0 to 1) goes from +z to -z
    maxAxis = absY;
    uc = x;
    vc = -z;
    index = 2;
  }
  // NEGATIVE Y
  else if (!isYPositive && absY >= absX && absY >= absZ) {
    // u (0 to 1) goes from -x to +x
    // v (0 to 1) goes from -z to +z
    maxAxis = absY;
    uc = x;
    vc = z;
    index = 3;
  }
  // POSITIVE Z
  else if (isZPositive && absZ >= absX && absZ >= absY) {
    // u (0 to 1) goes from -x to +x
    // v (0 to 1) goes from -y to +y
    maxAxis = absZ;
    uc = x;
    vc = y;
    index = 4;
  }
  // NEGATIVE Z
  else {
    // u (0 to 1) goes from +x to -x
    // v (0 to 1) goes from -y to +y
    maxAxis = absZ;
    uc = -x;
    vc = y;
    index = 5;
  }
  
  // Convert range from -1 to 1 to 0 to 1
  uv.x = 0.5 * (uc / maxAxis + 1.0);
  uv.y = 0.5 * (vc / maxAxis + 1.0);
 
  // Fix pixel border bullshit
  float pixelSize = 1.0 / 1024.0;

  uv *= vec2(1.0 - 2.0 * pixelSize);
  uv += vec2(pixelSize);  
}

void main() {
	// Add UV based on rotation so the sky rotates (tiling)
	// Zangle is between 0 and 2PI
	float SunIntensity = max(SunHeight, 0.0);
	vec4 starsColor = texture(StarfieldTexture, fUV).rgba;
	float positionInput = (fPosition.z - fPosition.x - fPosition.y) * 700.0;
	float sparkle = (sin(Time * 1.25 + positionInput) + 1.3) * 0.434782;
	float starIntensity = pow(1.0 - SunIntensity, 20.0);
	oColor.rgb += starsColor.rgb * starsColor.a * sparkle * starIntensity;
    
    // Clouds
    int index;
    vec2 uv;
    convert_xyz_to_cube_uv(fSkyVector.x, fSkyVector.y, fSkyVector.z, index, uv);
    
    // TODO: Fix stars
    oColor.rgb = oColor.rgb * 0.00001;
    
    switch (index) {
        case 0:
            oColor.rgb += texture(SkyboxRight, vec2(uv.y, 1.0 - uv.x)).rgb;
            break;
        case 1:
            oColor.rgb += texture(SkyboxLeft, vec2(-uv.y, 1.0 - uv.x)).rgb;
            break;
        case 2:
            oColor.rgb += texture(SkyboxFront, vec2(-uv.x, 1.0 - uv.y)).rgb;
            break;
        case 3:
            oColor.rgb += texture(SkyboxBack, vec2(uv.x, 1.0 - uv.y)).rgb;
            break;
        case 4:
           oColor.rgb += texture(SkyboxUp, vec2(uv.y, 1.0 - uv.x)).rgb;
           break;
        case 5:
           oColor.rgb = vec3(0.0);
           break;
    }
}