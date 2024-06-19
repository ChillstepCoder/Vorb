#pragma once
constexpr i32 MAX_DAMAGE_ZONE_RADIAL_SECTORS = 8;
constexpr i32 MAX_DAMAGE_ZONE_SLICES = 4;
constexpr i32 MAX_DAMAGE_ZONES = MAX_DAMAGE_ZONE_RADIAL_SECTORS * MAX_DAMAGE_ZONE_SLICES;

using TileDamageDataPtr = std::unique_ptr<class TileDamageData>;

using TileDamageZonesArray = std::array<ui8, MAX_DAMAGE_ZONES>;

struct TileDamageResistances {
    f32 outer = 5.0f;
    f32 inner = 0.0f;
    f32 blendExponent = 2.0f;
    i32 shellDegradeMultiplier = 1; // Larger = faster shell degrade
};

class TileDamageData {
public:
    TileDamageData() = default;
    ~TileDamageData() = default;
    TileDamageData(ui16 currentHealth) : mCurrentHealth(currentHealth) {}

    POOLED_ALLOC_DECL();

    static TileDamageDataPtr create(ui16 currentHealth = std::numeric_limits<ui16>::max()) {
        return std::make_unique<TileDamageData>(currentHealth);
    }

    // Returns applied damage as a positive integer
    i32 applyDamageStrike(f32v2 strikeNormal, f32 strikeRelativeZ, i32 baseStrikeDamage, TileDamageResistances tileResistance);

    auto operator<=>(const TileDamageData&) const = default;

    const TileDamageZonesArray& getShellDamageZones() const { return mShellDamageZones; }
    ui16 getCurrentHealth() const { return mCurrentHealth; }

private:
    TileDamageZonesArray mShellDamageZones = {}; // 0 = undamaged 255 = max damage
    ui16 mCurrentHealth = std::numeric_limits<ui16>::max();
};

