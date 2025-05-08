#pragma once

enum class SettlementZone : ui8 {
    Government = BIT(0),
    UrbanResidential = BIT(1),
    UrbanCommercial = BIT(2),
    Rural = BIT(3), // Farms, woodcutters, ect
    TERM = BIT(4), // KEEP UPDATED
    INVALID = TERM,
    ALL = 0xff
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
    static_assert(e_cast(SettlementZone::TERM) == BIT(4));
    return color::White;
}

struct SettlementPlotRequest {
    i32 minimumWidth = 5;
    i32 minimumSize = SQ(6);
    i32 maximumSize = SQ(10);
    BitFlags<SettlementZone> allowedZones = SettlementZone::ALL;
    // DTileCoord desiredProximity (for generating close to forests?)
};
