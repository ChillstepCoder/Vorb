#pragma once

enum class StatsType {
    Strength,
    Endurance,
    Agility,
    COUNT
};

struct StatsComponent {
    ui16 mStats[e_cast(StatsType::COUNT)];
};

