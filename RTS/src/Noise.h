///
/// Noise.h
/// From Seed of Andromeda
///
/// Created by Benjamin Arnold on 8 Jul 2015
/// Copyright 2014 Regrowth Studios
/// MIT License
///
/// 
///

#pragma once

#ifndef Noise_h__
#define Noise_h__

enum class TerrainStage {
	NOISE,
	SQUARED,
	CUBED,
	RIDGED_NOISE,
	ABS_NOISE,
	SQUARED_NOISE,
	CUBED_NOISE,
	CELLULAR_NOISE,
	CELLULAR_SQUARED_NOISE,
	CELLULAR_CUBED_NOISE,
	CONSTANT,
	PASS_THROUGH
};
KEG_ENUM_DECL(TerrainStage);

enum class TerrainOp {
	ADD = 0,
	SUB,
	MUL,
	DIV
};
KEG_ENUM_DECL(TerrainOp);

struct TerrainFuncProperties {
	TerrainStage func = TerrainStage::NOISE;
	TerrainOp op = TerrainOp::ADD;
	int octaves = 1;
	f64 persistence = 1.0;
	f64 frequency = 1.0;
	f64 low = -1.0;
	f64 high = 1.0;
	f64v2 clamp = f64v2(0.0);
	Array<TerrainFuncProperties> children;
};
KEG_TYPE_DECL(TerrainFuncProperties);

struct NoiseBase {
	f64 base = 0.0f;
	Array<TerrainFuncProperties> funcs;
};
KEG_TYPE_DECL(NoiseBase);

namespace Noise {
	f64v2 cellular(const f64v3& P);

	// Mulit-octave simplex noise
	f64 fractal(const int octaves,
		const f64 persistence,
		const f64 freq,
		const f64 x,
		const f64 y);
	f64 fractal(const int octaves,
		const f64 persistence,
		const f64 freq,
		const f64 x,
		const f64 y,
		const f64 z);
	f64 fractal(const int octaves,
		const f64 persistence,
		const f64 freq,
		const f64 x,
		const f64 y,
		const f64 z,
		const f64 w);

	// Raw Simplex noise - a single noise value.
	f64 raw(const f64 x, const f64 y);
	f64 raw(const f64 x, const f64 y, const f64 z);
	f64 raw(const f64 x, const f64 y, const f64, const f64 w);

	// Scaled Multi-octave Simplex noise
	// The result will be between the two parameters passed.
	inline f64 scaledFractal(const int octaves, const f64 persistence, const f64 freq, const f64 loBound, const f64 hiBound, const f64 x, const f64 y) {
		return fractal(octaves, persistence, freq, x, y) * (hiBound - loBound) / 2 + (hiBound + loBound) / 2;
	}
	inline f64 scaledFractal(const int octaves, const f64 persistence, const f64 freq, const f64 loBound, const f64 hiBound, const f64 x, const f64 y, const f64 z) {
		return fractal(octaves, persistence, freq, x, y, z) * (hiBound - loBound) / 2 + (hiBound + loBound) / 2;
	}
	inline f64 scaledFractal(const int octaves, const f64 persistence, const f64 freq, const f64 loBound, const f64 hiBound, const f64 x, const f64 y, const f64 z, const f64 w) {
		return fractal(octaves, persistence, freq, x, y, z, w) * (hiBound - loBound) / 2 + (hiBound + loBound) / 2;
	}

	// Scaled Raw Simplex noise
	// The result will be between the two parameters passed.
	inline f64 scaledRaw(const f64 loBound, const f64 hiBound, const f64 x, const f64 y) {
		return raw(x, y) * (hiBound - loBound) / 2 + (hiBound + loBound) / 2;
	}
	inline f64 scaledRaw(const f64 loBound, const f64 hiBound, const f64 x, const f64 y, const f64 z) {
		return raw(x, y, z) * (hiBound - loBound) / 2 + (hiBound + loBound) / 2;
	}
	inline f64 scaledRaw(const f64 loBound, const f64 hiBound, const f64 x, const f64 y, const f64 z, const f64 w) {
		return raw(x, y, z, w) * (hiBound - loBound) / 2 + (hiBound + loBound) / 2;
	}
}

#endif // Noise_h__