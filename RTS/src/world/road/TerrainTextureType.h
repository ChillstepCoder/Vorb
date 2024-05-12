#pragma once

// TODO: Data drive this?
enum class TerrainTextureType : ui8 {
    None,
    Dirt,
    FarmPlot,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(TerrainTextureType,
    pair{ TerrainTextureType::None, "None"sv },
    pair{ TerrainTextureType::Dirt, "Dirt"sv },
    pair{ TerrainTextureType::FarmPlot, "FarmPlot"sv }
)
