#pragma once

enum class FactionAttitude {
    Hostile,
    Wary,
    Neutral,
    Friendly,
    Allied
};

enum class FactionTraits : ui64 {
    Player = BIT(0),
    ChernobogCult = BIT(1),
    BanshiraCult = BIT(2),
    EarthCult = BIT(3),
};
constexpr ui64 FACTION_TRAIT_ALLEGIANCE_MASK = e_cast(FactionTraits::Player) | e_cast(FactionTraits::ChernobogCult) | e_cast(FactionTraits::BanshiraCult) | e_cast(FactionTraits::EarthCult);

// Copied while under lock
struct FactionThreadSafeData {
    const char* name = "UNKNOWN"; // TODO: Some kind of dictionary based read only string. ui32 charOffset + ui16 wordLength?
    color3 factionColor = color3(255, 0, 255);
    BitFlags<FactionTraits> traits;
};

struct Faction {
    std::vector<entt::entity> settlements;
    std::vector<entt::entity> members;

    // Fairly const, can be copied
    FactionThreadSafeData threadSafeData;
};