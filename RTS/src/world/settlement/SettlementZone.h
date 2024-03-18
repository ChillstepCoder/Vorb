#pragma once

enum class SettlementZone : ui8 {
    Government,
    UrbanResidential,
    UrbanCommercial,
    Rural, // Farms, woodcutters, ect
    COUNT
};

inline constexpr color4 getSettlementZoneDebugColor(SettlementZone zone) {
    switch (zone) {
        case SettlementZone::Government:
            return color::Purple;
        case SettlementZone::UrbanResidential:
            return color::Blue;
        case SettlementZone::UrbanCommercial:
            return color::Yellow;
        case SettlementZone::Rural:
            return color::LawnGreen;
        default:
            break;

    }
    static_assert(e_count(SettlementZone) == 4);
    return color::White;
}