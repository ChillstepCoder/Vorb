#pragma once

enum class ProfessionType {
    NONE,
    BUILDER,
    FARMER,
    HUNTER,
    SOLDIER,
    LUMBERJACK,
    MINER,
    TRADER,
    CLOTHIER,
    BLACKSMITH,
    FISHERMAN,
    BOWYER,
    ADVENTURER, // Rare
    BOUNTY_HUNTER, // Rare
    TYPES
};


struct ProfessionComponent
{
    ProfessionType mMainProfession;
    ProfessionType mSecondaryProfession;
};

