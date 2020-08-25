#pragma once
#include <cstdio>

inline void printMatrix(const char* title, const f32m4& mat) {
	printf(
		"%s:\n%f %f %f %f\n%f %f %f %f\n %f %f %f %f\n %f %f %f %f\n",
		title,
		mat[0][0],
		mat[0][1],
		mat[0][2],
		mat[0][3],
		mat[1][0],
		mat[1][1],
		mat[1][2],
		mat[1][3],
		mat[2][0],
		mat[2][1],
		mat[2][2],
		mat[2][3],
		mat[3][0],
		mat[3][1],
		mat[3][2],
		mat[3][3]
	);
}

inline f32v4 multiplyAndPrint4(const char* title, const f32v4& vec, const f32m4& mat) {
	printf(
		"PRE %s: <%f, %f, %f>\n",
		title,
		vec.x,
		vec.y,
		vec.z
	);
	f32v4 result = vec * mat;
	printf(
		"POST %s: <%f, %f, %f>\n",
		title,
		result.x,
		result.y,
		result.z
	);
	return result;
}

inline f32v2 multiplyAndPrint(const char* title, const f32v2& vec, const f32m4& mat) {
	printf(
		"PRE %s: <%f, %f>\n",
		title,
		vec.x,
		vec.y
	);
	f32v2 result = f32v4(vec, 0.0f, 0.0f) * mat;
	printf(
		"POST %s: <%f, %f>\n",
		title,
		result.x,
		result.y
	);
	return result;
}