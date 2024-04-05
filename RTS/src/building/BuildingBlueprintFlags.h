#pragma once

enum class BuildingBlueprintFlags : ui8 {
    BLUEPRINT_FLAG_CREATE_EARLY_STOCKPILE = BIT(0),
    BLUEPRINT_FLAG_FAILED_TO_GENERATE = BIT(1)
};

