uniform sampler2D FboZCutout;
uniform sampler2DArray Atlas;
uniform float Time;
uniform float ZoomScale;
uniform vec3 PlayerPosWorld;
uniform vec2 MousePosWorld;

in vec2 fUV;
in vec2 fUVZCutout;
flat in float fAtlasPage;
in vec4 fTint;
in vec2 fScreenPos;
in vec3 fWorldPos;

layout (location = 0) out vec4 fColor;
layout (location = 1) out vec4 fNormal;

// ==============================
// TODO: Use a stencil buffer of the bottom layer of tiles to override those as always pure alpha, so 
// we only reveal when the terrain is passable to show where you can go.

// Pseudo random number generator. 
float hash( vec2 a )
{

    return fract( sin( a.x * 3433.8 + a.y * 3843.98 ) * 45933.8 );

}

// TODO: Precompute the texture
// Value noise courtesy of BigWingz 
// check his youtube channel he has
// a video of this one.
// Succint version by FabriceNeyret
float vnoise( vec2 U )
{
    vec2 id = floor( U );
          U = fract( U );
    U *= U * ( 3. - 2. * U );  

    vec2 A = vec2( hash(id)            , hash(id + vec2(0,1)) ),  
         B = vec2( hash(id + vec2(1,0)), hash(id + vec2(1,1)) ),  
         C = mix( A, B, U.x);

    return mix( C.x, C.y, U.y );
}

const float CONE_WIDTH = 0.05;
const float CONE_SLOPE = 0.2;

// https://stackoverflow.com/questions/849211/shortest-distance-between-a-point-and-a-line-segment
float distanceToSegment(vec2 v, vec2 w, vec2 p) {
  // Return minimum distance between line segment vw and point p
  vec2 offset = w - v;
  float l2 = dot(offset, offset);  // avoid a sqrt
  //if (l2 == 0.0)  {
  //    return distance(p, v);   // v == w case
  //}
  float t = max(0.0, min(1.0, dot(p - v, offset) / l2));
  vec2 projection = v + t * offset;  // Projection falls on the segment

  return length(p - projection) * CONE_WIDTH + t * CONE_SLOPE;
}

const float XY_SCALE = 2.0;
const float XY_SCALE_ADJUST = -0.5;
const float OCCLUSION_EXPONENT = 4.0;
const float Z_SCALE = 0.3;
const float Z_SCALE_ADJUST = -0.75; // 0.5 for better walls but worse trees... TODO: Contextual??
const float NOISE_FREQUENCY = 3600.0;
const float XY_NOISE_SCALE = 0.15;
const float Z_NOISE_SCALE = 0.15;
const float NOISE_MODULATE_SPEED = 2.0;
const float Z_CUTOUT_NOISE_SCALE = 0.0008;
const float PLAYER_OFFSET_SCALE = 0.25;
const float MOUSE_OFFSET_SCALE = 0.25;


float computeOcclusionAlpha() {
	//vec2 playerOffset = fWorldPos.xy - PlayerPosWorld.xy;
	//float offsetMag = length(playerOffset) * PLAYER_OFFSET_SCALE;
	
	//vec2 mouseOffset = fWorldPos.xy - MousePosWorld.xy;
	//offsetMag = min(offsetMag, length(mouseOffset) * MOUSE_OFFSET_SCALE);
	
	float offsetMag = distanceToSegment(MousePosWorld.xy, PlayerPosWorld.xy, fWorldPos.xy);
	
	
	// Reduce radius based on zoom scale
	//offsetMag -= (ZoomScale - 50.0) * 0.01;
	
	float noise = vnoise(fScreenPos.xy * NOISE_FREQUENCY + Time * NOISE_MODULATE_SPEED - fWorldPos.z);
	offsetMag += noise * XY_NOISE_SCALE;
	// [0,1] Represents xy offset length scaled
	float xyScaled = (offsetMag * XY_SCALE + XY_SCALE_ADJUST);
	
	// Higher we are from player position, more that alpha will contribute
	// [0,1] Represents Z height as a scale
	float zScaled = (fWorldPos.z - PlayerPosWorld.z) * Z_SCALE + Z_SCALE_ADJUST + noise * Z_NOISE_SCALE; // noise * Z_NOISE_SCALE should be redundant with mask
	
	float alpha = clamp(max(1.0 - zScaled, xyScaled), 0.0, 1.0);
	
	alpha = pow(alpha, OCCLUSION_EXPONENT);
	
	// Use cutout so we can display transparency properly.
	// Only show transparency when there is actually something to reveal.
	vec2 zUV = fUVZCutout;
	zUV.y -= (noise + 1.0) * Z_CUTOUT_NOISE_SCALE * ZoomScale;
	float zCutoutAlpha = 1.0 - min(texture(FboZCutout, zUV).r, texture(FboZCutout, fUVZCutout).r);
	return max(zCutoutAlpha, alpha);
}
// NEED: 1,1,1,0
// xy + z = 1,2,0,1
// z * xy = 0,1,0,0
// max(1.0 - z, xy) = 1,1,1,0 // WOOH
// When in doubt use a truth table for formula
// result = func(xyScaled, zScaled);
//
// zScaled = 0.0 (bottom layer)
// xyScaled = 1.0 // far away
// Result should be 1.0

// zScaled = 1.0 (top layer)
// xyScaled = 1.0 // far away
// Result should be 1.0

// zScaled = 0.0 (bottom layer)
// xyScaled = 0.0 // close
// Result should be 1.0

// zScaled = 1.0 (top layer)
// xyScaled = 0.0 // close
// Result should be 0.0 // INVISIBLE

void main() {
    fColor = texture(Atlas, vec3(fUV, fAtlasPage)) * fTint;
	fColor.a *= computeOcclusionAlpha();
    // Don't write 0 alpha (TMP?)
	// TODO: Noise on this edge so that its fuzzy average
	
    if (fColor.a <= 0.99) {
        discard;
    }
	
	
	// Normal is always the next page
	fNormal = texture(Atlas, vec3(fUV, fAtlasPage + 1.0));
	fNormal.a = fColor.a;
}