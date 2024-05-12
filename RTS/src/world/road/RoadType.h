#pragma once

// TODO: Data drive this?
enum class RoadType : ui8 {
    Dirt,
    FarmPlot,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(RoadType,
    pair{ RoadType::Dirt, "Dirt"sv },
    pair{ RoadType::FarmPlot, "FarmPlot"sv }
)
