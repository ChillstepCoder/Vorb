#include "stdafx.h"
#include "RenderStats.h"

ui32 RenderStats::sDrawCalls = 0;
ui32 RenderStats::sPolyCount = 0;

void RenderStats::clear() {
    sDrawCalls = 0;
    sPolyCount = 0;
}

void RenderStats::recordDrawCall(ui32 polyCount) {
    ++sDrawCalls;
    sPolyCount += polyCount;
}
