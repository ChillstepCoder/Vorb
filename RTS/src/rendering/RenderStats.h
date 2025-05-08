#pragma once

class RenderStats {
public:
    static void clear();

    static void recordDrawCall(ui32 polyCount);

    static ui32 sDrawCalls;
    static ui32 sPolyCount;
};