#include "stdafx.h"


bool IS_SHUTTING_DOWN = false;

UNIT_SPACE(SECONDS) f64 sTotalTimeSeconds; ///< Total time since the update/draw loop started.
UNIT_SPACE(SECONDS) f32 sElapsedSecondsSinceLastFrame; ///< Elapsed time of the previous frame.

float sFps = 0.0f;
