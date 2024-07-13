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
SERIALIZABLE_ENUM_SAME_NAME(TerrainStage,
    pair{ TerrainStage::NOISE, "noise"sv },
	pair{ TerrainStage::SQUARED, "squared"sv },
	pair{ TerrainStage::CUBED, "cubed"sv },
	pair{ TerrainStage::RIDGED_NOISE, "noise_ridged"sv },
	pair{ TerrainStage::ABS_NOISE, "noise_abs"sv },
	pair{ TerrainStage::SQUARED_NOISE, "noise_squared"sv },
	pair{ TerrainStage::CUBED_NOISE, "noise_cubed"sv },
	pair{ TerrainStage::CELLULAR_NOISE, "noise_cellular"sv },
	pair{ TerrainStage::CELLULAR_SQUARED_NOISE, "noise_cellular_squared"sv },
	pair{ TerrainStage::CELLULAR_CUBED_NOISE, "noise_cellular_cubed"sv },
	pair{ TerrainStage::CONSTANT, "constant"sv },
	pair{ TerrainStage::PASS_THROUGH, "passthrough"sv }
);

enum class TerrainOp {
	ADD = 0,
	SUB,
	MUL,
	DIV
};
SERIALIZABLE_ENUM_SAME_NAME(TerrainOp,
    pair{ TerrainOp::ADD, "add"sv },
    pair{ TerrainOp::SUB, "sub"sv },
    pair{ TerrainOp::MUL, "mul"sv },
    pair{ TerrainOp::DIV, "div"sv }
);

struct TerrainFuncProperties {
	TerrainStage func = TerrainStage::NOISE;
	TerrainOp op = TerrainOp::ADD;
	int octaves = 1;
	f64 persistence = 1.0;
	f64 frequency = 1.0;
	f64 low = -1.0;
	f64 high = 1.0;
	f64v2 clamp = f64v2(0.0);
	std::vector<TerrainFuncProperties> children;
};
SERIALIZABLE_SIMPLE(TerrainFuncProperties,
	make_field(o.func, "type"sv),
	make_field(o.op, "op"sv),
	make_field(o.octaves, "octaves"sv),
	make_field(o.persistence, "persistence"sv),
	make_field(o.frequency, "frequency"sv),
	make_field(o.low, "low"sv),
	make_field(o.high, "high"sv),
	make_field(o.clamp, "clamp"sv),
	make_field(o.children, "children"sv)
)

struct NoiseBase {
	f64 base = 0.0f;
	std::vector<TerrainFuncProperties> funcs;
};

namespace Noise {
    f64v2 cellularEuclidean(const f64v2& P);
    f64v2 cellularManhattan(const f64v2& P);

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