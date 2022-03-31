#pragma once

enum class StatsType {
    STRENGTH,
    ENDURANCE,
    AGILITY,
    COUNT
};

struct StatsComponent
{


    ui32 mStats[e_cast(StatsType::COUNT)]; //[Curr, Max]
};

