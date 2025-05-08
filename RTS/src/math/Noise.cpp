#include "stdafx.h"
#include "math/Noise.h"

#include <Vorb/utils.h>

constexpr f64 SQRT_3 = 1.7320508075688772935274463415058723669428052538103806280558069794; // Eliminate sqrt3
constexpr f64 SQRT_5 = 2.2360679774997896964091736687312762354406183596115257242708972454; // Eliminate sqrt5

// The gradients are the midpoints of the vertices of a cube.
constexpr f64 grad3[12][3] = {
    { 1, 1, 0 }, { -1, 1, 0 }, { 1, -1, 0 }, { -1, -1, 0 },
    { 1, 0, 1 }, { -1, 0, 1 }, { 1, 0, -1 }, { -1, 0, -1 },
    { 0, 1, 1 }, { 0, -1, 1 }, { 0, 1, -1 }, { 0, -1, -1 }
};

// The gradients are the midpoints of the vertices of a hypercube.
constexpr f64 grad4[32][4] = {
    { 0, 1, 1, 1 }, { 0, 1, 1, -1 }, { 0, 1, -1, 1 }, { 0, 1, -1, -1 },
    { 0, -1, 1, 1 }, { 0, -1, 1, -1 }, { 0, -1, -1, 1 }, { 0, -1, -1, -1 },
    { 1, 0, 1, 1 }, { 1, 0, 1, -1 }, { 1, 0, -1, 1 }, { 1, 0, -1, -1 },
    { -1, 0, 1, 1 }, { -1, 0, 1, -1 }, { -1, 0, -1, 1 }, { -1, 0, -1, -1 },
    { 1, 1, 0, 1 }, { 1, 1, 0, -1 }, { 1, -1, 0, 1 }, { 1, -1, 0, -1 },
    { -1, 1, 0, 1 }, { -1, 1, 0, -1 }, { -1, -1, 0, 1 }, { -1, -1, 0, -1 },
    { 1, 1, 1, 0 }, { 1, 1, -1, 0 }, { 1, -1, 1, 0 }, { 1, -1, -1, 0 },
    { -1, 1, 1, 0 }, { -1, 1, -1, 0 }, { -1, -1, 1, 0 }, { -1, -1, -1, 0 }
};


// Permutation table.  The same list is repeated twice.
constexpr int perm[512] = {
    151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142,
    8, 99, 37, 240, 21, 10, 23, 190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117,
    35, 11, 32, 57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71,
    134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41,
    55, 46, 245, 40, 244, 102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89,
    18, 169, 200, 196, 135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226,
    250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182,
    189, 28, 42, 223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43,
    172, 9, 129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97,
    228, 251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239,
    107, 49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
    138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180,

    151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142,
    8, 99, 37, 240, 21, 10, 23, 190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117,
    35, 11, 32, 57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71,
    134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41,
    55, 46, 245, 40, 244, 102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89,
    18, 169, 200, 196, 135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226,
    250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182,
    189, 28, 42, 223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43,
    172, 9, 129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97,
    228, 251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239,
    107, 49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
    138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180
};


// A lookup table to traverse the simplex around a given point in 4D.
constexpr int simplex[64][4] = {
    { 0, 1, 2, 3 }, { 0, 1, 3, 2 }, { 0, 0, 0, 0 }, { 0, 2, 3, 1 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 1, 2, 3, 0 },
    { 0, 2, 1, 3 }, { 0, 0, 0, 0 }, { 0, 3, 1, 2 }, { 0, 3, 2, 1 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 1, 3, 2, 0 },
    { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
    { 1, 2, 0, 3 }, { 0, 0, 0, 0 }, { 1, 3, 0, 2 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 2, 3, 0, 1 }, { 2, 3, 1, 0 },
    { 1, 0, 2, 3 }, { 1, 0, 3, 2 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 2, 0, 3, 1 }, { 0, 0, 0, 0 }, { 2, 1, 3, 0 },
    { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
    { 2, 0, 1, 3 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 3, 0, 1, 2 }, { 3, 0, 2, 1 }, { 0, 0, 0, 0 }, { 3, 1, 2, 0 },
    { 2, 1, 0, 3 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 3, 1, 0, 2 }, { 0, 0, 0, 0 }, { 3, 2, 0, 1 }, { 3, 2, 1, 0 }
};

//
// Description : Array and textureless 2D/3D/4D simplex 
//               noise functions.
//      Author : Ian McEwan, Ashima Arts.
//  Maintainer : ijm
//     Lastmod : 20110822 (ijm)
//     License : Copyright (C) 2011 Ashima Arts. MIT License.
//               Distributed under the MIT License. See LICENSE file.
//               https://github.com/ashima/webgl-noise
// 
// Converted to C++ by Ben Arnold
// Modulo 289 without a division (only multiplications)
f64v3 mod289(f64v3 x) {
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

f64v2 mod289(f64v2 x) {
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

// Modulo 7 without a division
f64v3 mod7(f64v3 x) {
    return x - floor(x * (1.0 / 7.0)) * 7.0;
}

// Permutation polynomial: (34x^2 + 6x) mod 289
f64v3 permute(f64v3 x) {
    return mod289((34.0 * x + 10.0) * x);
}

// https://github.com/ashima/webgl-noise/blob/master/src/cellular2D.glsl
f64v2 Noise::cellularEuclidean(const f64v2& P) {
#define K 0.142857142857 // 1/7
#define Ko 0.428571428571 // 3/7
#define jitter 1.0 // Less gives more regular pattern
    f64v2 Pi = mod289(floor(P));
	f64v2 Pf = fract(P);
    f64v3 oi = f64v3(-1.0, 0.0, 1.0);
    f64v3 of = f64v3(-0.5, 0.5, 1.5);
    f64v3 px = permute(Pi.x + oi);
    f64v3 p = permute(px.x + Pi.y + oi); // p11, p12, p13
	f64v3 ox = glm::fract(p * K) - Ko;
    f64v3 oy = mod7(floor(p * K)) * K - Ko;
    f64v3 dx = Pf.x + 0.5f + jitter * ox;
    f64v3 dy = Pf.y - of + jitter * oy;
    f64v3 d1 = dx * dx + dy * dy; // d11, d12 and d13, squared
    p = permute(px.y + Pi.y + oi); // p21, p22, p23
    ox = glm::fract(p * K) - Ko;
    oy = mod7(floor(p * K)) * K - Ko;
    dx = Pf.x - 0.5 + jitter * ox;
    dy = Pf.y - of + jitter * oy;
    f64v3 d2 = dx * dx + dy * dy; // d21, d22 and d23, squared
    p = permute(px.z + Pi.y + oi); // p31, p32, p33
    ox = glm::fract(p * K) - Ko;
    oy = mod7(floor(p * K)) * K - Ko;
    dx = Pf.x - 1.5 + jitter * ox;
    dy = Pf.y - of + jitter * oy;
    f64v3 d3 = dx * dx + dy * dy; // d31, d32 and d33, squared
    // Sort out the two smallest distances (F1, F2)
    f64v3 d1a = glm::min(d1, d2);
    d2 = glm::max(d1, d2); // Swap to keep candidates for F2
    d2 = glm::min(d2, d3); // neither F1 nor F2 are now in d3
    d1 = glm::min(d1a, d2); // F1 is now in d1
    d2 = glm::max(d1a, d2); // Swap to keep candidates for F2
    if (d1.x > d1.y) d1 = f64v3(d1.y, d1.x, d1.z);
    if (d1.x > d1.z) d1 = f64v3(d1.z, d1.y, d1.x);
	d1.y = glm::min(d1.y, d2.y);
	d1.z = glm::min(d1.z, d2.z);
    d1.y = glm::min(d1.y, d1.z); // nor in  d1.z
    d1.y = glm::min(d1.y, d2.x); // F2 is in d1.y, we're done.
	return f64v2(sqrt(d1.x), sqrt(d1.y));
}

// Multi-octave Simplex noise
// For each octave, a higher frequency/lower amplitude function will be added to the original.
// The higher the persistence [0-1], the more of each succeeding octave will be added.

//SOURCE
// http://www.6by9.net/simplex-noise-for-c-and-python/


#define offsetfmult 1.45

inline i64 fastfloor(const f64 x) { return x > 0 ? (i64)x : (i64)x - 1; }

inline f64 dot(const f64* g, const f64 x, const f64 y) { return g[0] * x + g[1] * y; }
inline f64 dot(const f64* g, const f64 x, const f64 y, const f64 z) { return g[0] * x + g[1] * y + g[2] * z; }
inline f64 dot(const f64* g, const f64 x, const f64 y, const f64 z, const f64 w) { return g[0] * x + g[1] * y + g[2] * z + g[3] * w; }

f64 Noise::fractal(const int octaves, const f64 persistence, const f64 freq, const f64 x, const f64 y) {
	f64 total = 0.0;
	f64 frequency = freq;
	f64 amplitude = 1.0;

	// We have to keep track of the largest possible amplitude,
	// because each octave adds more, and we need a value in [-1, 1].
	f64 maxAmplitude = 0.0;

	for (int i = 0; i < octaves; i++) {
		total += raw(x * frequency, y * frequency) * amplitude;

		frequency *= 2.0;
		maxAmplitude += amplitude;
		amplitude *= persistence;
	}

	return total / maxAmplitude;
}

f64 Noise::fractal(const int octaves, const f64 persistence, const f64 freq, const f64 x, const f64 y, const f64 z) {
	f64 total = 0.0;
	f64 frequency = freq;
	f64 amplitude = 1.0;

	// We have to keep track of the largest possible amplitude,
	// because each octave adds more, and we need a value in [-1, 1].
	f64 maxAmplitude = 0.0;

	for (int i = 0; i < octaves; i++) {
		total += raw(x * frequency, y * frequency, z * frequency) * amplitude;

		frequency *= 2.0;
		maxAmplitude += amplitude;
		amplitude *= persistence;
	}

	return total / maxAmplitude;
}

f64 Noise::fractal(const int octaves, const f64 persistence, const f64 freq, const f64 x, const f64 y, const f64 z, const f64 w) {
	f64 total = 0;
	f64 frequency = freq;
	f64 amplitude = 1;

	// We have to keep track of the largest possible amplitude,
	// because each octave adds more, and we need a value in [-1, 1].
	f64 maxAmplitude = 0;

	for (int i = 0; i < octaves; i++) {
		total += raw(x * frequency, y * frequency, z * frequency, w * frequency) * amplitude;

		frequency *= 2.0;
		maxAmplitude += amplitude;
		amplitude *= persistence;
	}

	return total / maxAmplitude;
}

// 2D raw Simplex noise
f64 Noise::raw(const f64 x, const f64 y) {
	// Noise contributions from the three corners
	f64 n0, n1, n2;

	// Skew the input space to determine which simplex cell we're in
	f64 F2 = 0.5 * (SQRT_3 - 1.0);
	// Hairy factor for 2D
	f64 s = (x + y) * F2;
	i64 i = fastFloor(x + s);
	i64 j = fastFloor(y + s);

	f64 G2 = (3.0 - SQRT_3) / 6.0;
	f64 t = (i + j) * G2;
	// Unskew the cell origin back to (x,y) space
	f64 X0 = i - t;
	f64 Y0 = j - t;
	// The x,y distances from the cell origin
	f64 x0 = x - X0;
	f64 y0 = y - Y0;

	// For the 2D case, the simplex shape is an equilateral triangle.
	// Determine which simplex we are in.
	int i1, j1; // Offsets for second (middle) corner of simplex in (i,j) coords
	if (x0 > y0) { i1 = 1; j1 = 0; } // lower triangle, XY order: (0,0)->(1,0)->(1,1)
	else { i1 = 0; j1 = 1; } // upper triangle, YX order: (0,0)->(0,1)->(1,1)

	// A step of (1,0) in (i,j) means a step of (1-c,-c) in (x,y), and
	// a step of (0,1) in (i,j) means a step of (-c,1-c) in (x,y), where
	// c = (3-sqrt(3))/6
	f64 x1 = x0 - i1 + G2; // Offsets for middle corner in (x,y) unskewed coords
	f64 y1 = y0 - j1 + G2;
	f64 x2 = x0 - 1.0 + 2.0 * G2; // Offsets for last corner in (x,y) unskewed coords
	f64 y2 = y0 - 1.0 + 2.0 * G2;

	// Work out the hashed gradient indices of the three simplex corners
	int ii = i & 255;
	int jj = j & 255;
	int gi0 = perm[ii + perm[jj]] % 12;
	int gi1 = perm[ii + i1 + perm[jj + j1]] % 12;
	int gi2 = perm[ii + 1 + perm[jj + 1]] % 12;

	// Calculate the contribution from the three corners
	f64 t0 = 0.5 - x0 * x0 - y0 * y0;
	if (t0 < 0) n0 = 0.0;
	else {
		t0 *= t0;
		n0 = t0 * t0 * dot(grad3[gi0], x0, y0); // (x,y) of grad3 used for 2D gradient
	}

	f64 t1 = 0.5 - x1 * x1 - y1 * y1;
	if (t1 < 0) n1 = 0.0;
	else {
		t1 *= t1;
		n1 = t1 * t1 * dot(grad3[gi1], x1, y1);
	}

	f64 t2 = 0.5 - x2 * x2 - y2 * y2;
	if (t2 < 0) n2 = 0.0;
	else {
		t2 *= t2;
		n2 = t2 * t2 * dot(grad3[gi2], x2, y2);
	}

	// Add contributions from each corner to get the final noise value.
	// The result is scaled to return values in the interval [-1,1].
	return 70.0 * (n0 + n1 + n2);
}

// 3D raw Simplex noise
f64 Noise::raw(const f64 x, const f64 y, const f64 z) {
	f64 n0, n1, n2, n3; // Noise contributions from the four corners

	// Skew the input space to determine which simplex cell we're in
	const f64 F3 = 1.0 / 3.0;
	f64 s = (x + y + z) * F3; // Very nice and simple skew factor for 3D
	i64 i = fastFloor(x + s);
	i64 j = fastFloor(y + s);
	i64 k = fastFloor(z + s);

	const f64 G3 = 1.0 / 6.0; // Very nice and simple unskew factor, too
	f64 t = (i + j + k) * G3;
	f64 X0 = i - t; // Unskew the cell origin back to (x,y,z) space
	f64 Y0 = j - t;
	f64 Z0 = k - t;
	f64 x0 = x - X0; // The x,y,z distances from the cell origin
	f64 y0 = y - Y0;
	f64 z0 = z - Z0;

	// For the 3D case, the simplex shape is a slightly irregular tetrahedron.
	// Determine which simplex we are in.
	int i1, j1, k1; // Offsets for second corner of simplex in (i,j,k) coords
	int i2, j2, k2; // Offsets for third corner of simplex in (i,j,k) coords

	if (x0 >= y0) {
		if (y0 >= z0) { i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 1; k2 = 0; } // X Y Z order
		else if (x0 >= z0) { i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 0; k2 = 1; } // X Z Y order
		else { i1 = 0; j1 = 0; k1 = 1; i2 = 1; j2 = 0; k2 = 1; } // Z X Y order
	}
	else { // x0<y0
		if (y0 < z0) { i1 = 0; j1 = 0; k1 = 1; i2 = 0; j2 = 1; k2 = 1; } // Z Y X order
		else if (x0 < z0) { i1 = 0; j1 = 1; k1 = 0; i2 = 0; j2 = 1; k2 = 1; } // Y Z X order
		else { i1 = 0; j1 = 1; k1 = 0; i2 = 1; j2 = 1; k2 = 0; } // Y X Z order
	}

	// A step of (1,0,0) in (i,j,k) means a step of (1-c,-c,-c) in (x,y,z),
	// a step of (0,1,0) in (i,j,k) means a step of (-c,1-c,-c) in (x,y,z), and
	// a step of (0,0,1) in (i,j,k) means a step of (-c,-c,1-c) in (x,y,z), where
	// c = 1/6.
	f64 x1 = x0 - i1 + G3; // Offsets for second corner in (x,y,z) coords
	f64 y1 = y0 - j1 + G3;
	f64 z1 = z0 - k1 + G3;
	f64 x2 = x0 - i2 + 2.0 * G3; // Offsets for third corner in (x,y,z) coords
	f64 y2 = y0 - j2 + 2.0 * G3;
	f64 z2 = z0 - k2 + 2.0 * G3;
	f64 x3 = x0 - 1.0 + 3.0 * G3; // Offsets for last corner in (x,y,z) coords
	f64 y3 = y0 - 1.0 + 3.0 * G3;
	f64 z3 = z0 - 1.0 + 3.0 * G3;

	// Work out the hashed gradient indices of the four simplex corners
	int ii = i & 255;
	int jj = j & 255;
	int kk = k & 255;
	int gi0 = perm[ii + perm[jj + perm[kk]]] % 12;
	int gi1 = perm[ii + i1 + perm[jj + j1 + perm[kk + k1]]] % 12;
	int gi2 = perm[ii + i2 + perm[jj + j2 + perm[kk + k2]]] % 12;
	int gi3 = perm[ii + 1 + perm[jj + 1 + perm[kk + 1]]] % 12;

	// Calculate the contribution from the four corners
	f64 t0 = 0.6 - x0 * x0 - y0 * y0 - z0 * z0;
	if (t0 < 0) n0 = 0.0;
	else {
		t0 *= t0;
		n0 = t0 * t0 * dot(grad3[gi0], x0, y0, z0);
	}

	f64 t1 = 0.6 - x1 * x1 - y1 * y1 - z1 * z1;
	if (t1 < 0) n1 = 0.0;
	else {
		t1 *= t1;
		n1 = t1 * t1 * dot(grad3[gi1], x1, y1, z1);
	}

	f64 t2 = 0.6 - x2 * x2 - y2 * y2 - z2 * z2;
	if (t2 < 0) n2 = 0.0;
	else {
		t2 *= t2;
		n2 = t2 * t2 * dot(grad3[gi2], x2, y2, z2);
	}

	f64 t3 = 0.6 - x3 * x3 - y3 * y3 - z3 * z3;
	if (t3 < 0) n3 = 0.0;
	else {
		t3 *= t3;
		n3 = t3 * t3 * dot(grad3[gi3], x3, y3, z3);
	}

	// Add contributions from each corner to get the final noise value.
	// The result is scaled to stay just inside [-1,1]
	return 32.0 * (n0 + n1 + n2 + n3);
}

// 4D raw Simplex noise
f64 Noise::raw(const f64 x, const f64 y, const f64 z, const f64 w) {
	// The skewing and unskewing factors are hairy again for the 4D case
	f64 F4 = (SQRT_5 - 1.0) / 4.0;
	f64 G4 = (5.0 - SQRT_5) / 20.0;
	f64 n0, n1, n2, n3, n4; // Noise contributions from the five corners

	// Skew the (x,y,z,w) space to determine which cell of 24 simplices we're in
	f64 s = (x + y + z + w) * F4; // Factor for 4D skewing
	i64 i = fastFloor(x + s);
	i64 j = fastFloor(y + s);
	i64 k = fastFloor(z + s);
	i64 l = fastFloor(w + s);
	f64 t = (i + j + k + l) * G4; // Factor for 4D unskewing
	f64 X0 = i - t; // Unskew the cell origin back to (x,y,z,w) space
	f64 Y0 = j - t;
	f64 Z0 = k - t;
	f64 W0 = l - t;

	f64 x0 = x - X0; // The x,y,z,w distances from the cell origin
	f64 y0 = y - Y0;
	f64 z0 = z - Z0;
	f64 w0 = w - W0;

	// For the 4D case, the simplex is a 4D shape I won't even try to describe.
	// To find out which of the 24 possible simplices we're in, we need to
	// determine the magnitude ordering of x0, y0, z0 and w0.
	// The method below is a good way of finding the ordering of x,y,z,w and
	// then find the correct traversal order for the simplex we're in.
	// First, six pair-wise comparisons are performed between each possible pair
	// of the four coordinates, and the results are used to add up binary bits
	// for an integer index.
	int c1 = (x0 > y0) ? 32 : 0;
	int c2 = (x0 > z0) ? 16 : 0;
	int c3 = (y0 > z0) ? 8 : 0;
	int c4 = (x0 > w0) ? 4 : 0;
	int c5 = (y0 > w0) ? 2 : 0;
	int c6 = (z0 > w0) ? 1 : 0;
	int c = c1 + c2 + c3 + c4 + c5 + c6;

	int i1, j1, k1, l1; // The integer offsets for the second simplex corner
	int i2, j2, k2, l2; // The integer offsets for the third simplex corner
	int i3, j3, k3, l3; // The integer offsets for the fourth simplex corner

	// simplex[c] is a 4-vector with the numbers 0, 1, 2 and 3 in some order.
	// Many values of c will never occur, since e.g. x>y>z>w makes x<z, y<w and x<w
	// impossible. Only the 24 indices which have non-zero entries make any sense.
	// We use a thresholding to set the coordinates in turn from the largest magnitude.
	// The number 3 in the "simplex" array is at the position of the largest coordinate.
	i1 = simplex[c][0] >= 3 ? 1 : 0;
	j1 = simplex[c][1] >= 3 ? 1 : 0;
	k1 = simplex[c][2] >= 3 ? 1 : 0;
	l1 = simplex[c][3] >= 3 ? 1 : 0;
	// The number 2 in the "simplex" array is at the second largest coordinate.
	i2 = simplex[c][0] >= 2 ? 1 : 0;
	j2 = simplex[c][1] >= 2 ? 1 : 0;
	k2 = simplex[c][2] >= 2 ? 1 : 0;
	l2 = simplex[c][3] >= 2 ? 1 : 0;
	// The number 1 in the "simplex" array is at the second smallest coordinate.
	i3 = simplex[c][0] >= 1 ? 1 : 0;
	j3 = simplex[c][1] >= 1 ? 1 : 0;
	k3 = simplex[c][2] >= 1 ? 1 : 0;
	l3 = simplex[c][3] >= 1 ? 1 : 0;
	// The fifth corner has all coordinate offsets = 1, so no need to look that up.

	f64 x1 = x0 - i1 + G4; // Offsets for second corner in (x,y,z,w) coords
	f64 y1 = y0 - j1 + G4;
	f64 z1 = z0 - k1 + G4;
	f64 w1 = w0 - l1 + G4;
	f64 x2 = x0 - i2 + 2.0 * G4; // Offsets for third corner in (x,y,z,w) coords
	f64 y2 = y0 - j2 + 2.0 * G4;
	f64 z2 = z0 - k2 + 2.0 * G4;
	f64 w2 = w0 - l2 + 2.0 * G4;
	f64 x3 = x0 - i3 + 3.0 * G4; // Offsets for fourth corner in (x,y,z,w) coords
	f64 y3 = y0 - j3 + 3.0 * G4;
	f64 z3 = z0 - k3 + 3.0 * G4;
	f64 w3 = w0 - l3 + 3.0 * G4;
	f64 x4 = x0 - 1.0 + 4.0 * G4; // Offsets for last corner in (x,y,z,w) coords
	f64 y4 = y0 - 1.0 + 4.0 * G4;
	f64 z4 = z0 - 1.0 + 4.0 * G4;
	f64 w4 = w0 - 1.0 + 4.0 * G4;

	// Work out the hashed gradient indices of the five simplex corners
	int ii = i & 255;
	int jj = j & 255;
	int kk = k & 255;
	int ll = l & 255;
	int gi0 = perm[ii + perm[jj + perm[kk + perm[ll]]]] % 32;
	int gi1 = perm[ii + i1 + perm[jj + j1 + perm[kk + k1 + perm[ll + l1]]]] % 32;
	int gi2 = perm[ii + i2 + perm[jj + j2 + perm[kk + k2 + perm[ll + l2]]]] % 32;
	int gi3 = perm[ii + i3 + perm[jj + j3 + perm[kk + k3 + perm[ll + l3]]]] % 32;
	int gi4 = perm[ii + 1 + perm[jj + 1 + perm[kk + 1 + perm[ll + 1]]]] % 32;

	// Calculate the contribution from the five corners
	f64 t0 = 0.6 - x0 * x0 - y0 * y0 - z0 * z0 - w0 * w0;
	if (t0 < 0) n0 = 0.0;
	else {
		t0 *= t0;
		n0 = t0 * t0 * dot(grad4[gi0], x0, y0, z0, w0);
	}

	f64 t1 = 0.6 - x1 * x1 - y1 * y1 - z1 * z1 - w1 * w1;
	if (t1 < 0) n1 = 0.0;
	else {
		t1 *= t1;
		n1 = t1 * t1 * dot(grad4[gi1], x1, y1, z1, w1);
	}

	f64 t2 = 0.6 - x2 * x2 - y2 * y2 - z2 * z2 - w2 * w2;
	if (t2 < 0) n2 = 0.0;
	else {
		t2 *= t2;
		n2 = t2 * t2 * dot(grad4[gi2], x2, y2, z2, w2);
	}

	f64 t3 = 0.6 - x3 * x3 - y3 * y3 - z3 * z3 - w3 * w3;
	if (t3 < 0) n3 = 0.0;
	else {
		t3 *= t3;
		n3 = t3 * t3 * dot(grad4[gi3], x3, y3, z3, w3);
	}

	f64 t4 = 0.6 - x4 * x4 - y4 * y4 - z4 * z4 - w4 * w4;
	if (t4 < 0) n4 = 0.0;
	else {
		t4 *= t4;
		n4 = t4 * t4 * dot(grad4[gi4], x4, y4, z4, w4);
	}

	// Sum up and scale the result to cover the range [-1,1]
	return 27.0 * (n0 + n1 + n2 + n3 + n4);
}