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

struct Faction {
    nString name = "UNKNOWN";
    BitFlags<FactionTraits> traits;
    std::vector<entt::entity> settlements;
    std::vector<entt::entity> members;
};