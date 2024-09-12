#include "stdafx.h"
#include "TileDamageData.h"

POOLED_ALLOC_DEF_THREADSAFE(TileDamageData, 2048);

i32 TileDamageData::applyDamageStrike(f32v2 strikeNormal, f32 strikeRelativeZ, i32 baseStrikeDamage, TileDamageResistances tileResistance) {
    // TODO: Implement vertical later
    UNUSED(strikeRelativeZ);

    // Determine shell slice and calculate weights
    const float angle = atan2f(strikeNormal.y, strikeNormal.x) + M_PIF;
    const float sliceFloat = std::clamp(angle / (M_2_PIF / MAX_DAMAGE_ZONE_RADIAL_SECTORS), 0.0f, (f32)(MAX_DAMAGE_ZONE_RADIAL_SECTORS - 1));
    const int slice1 = int(floor(sliceFloat));
    float sliceFloatFract = sliceFloat - (float)slice1;

    // Potentially damage two slices based on angle
    float weight1;
    int slice2;
    constexpr f32 BLEND_THRESHOLD = 0.3f;
    if (sliceFloatFract < BLEND_THRESHOLD) {
        // Linear blend downward
        if (slice1 == 0) {
            slice2 = MAX_DAMAGE_ZONE_RADIAL_SECTORS - 1;
        } else {
            slice2 = slice1 - 1;
        }
        weight1 = 1.0f - ((BLEND_THRESHOLD - sliceFloatFract) / BLEND_THRESHOLD) * 0.5f;
    }
    else if (sliceFloatFract > 1.0f - BLEND_THRESHOLD) {
        // Linear blend upward
        slice2 = (slice1 + 1) % MAX_DAMAGE_ZONE_RADIAL_SECTORS;
        weight1 = 1.0f - ((sliceFloatFract - (1.0f - BLEND_THRESHOLD)) / BLEND_THRESHOLD) * 0.5f;
    }
    else {
        // No blend
        slice2 = slice1;
        weight1 = 1.0f;
    }

    float weight2 = 1.0f - weight1;

    // Returns <health damage, shell damage>
    auto applyDamageToSlice = [&](int slice, float weight) -> i32v2 {
        const ui8 currentShellDamage = mShellDamageZones[slice];
        const f32 currentSliceDamageRatio = currentShellDamage / 255.0f;
        const f32 currentResistance = util::lerp(tileResistance.outer, tileResistance.inner, powf(currentSliceDamageRatio, tileResistance.blendExponent));
        const f32 healthDamage = glm::max(baseStrikeDamage * weight - currentResistance, 0.0f);
        const f32 shellDamage = healthDamage * tileResistance.shellDegradeMultiplier;

        // Apply shell damage
        ui8 newShellDamage = (ui8)glm::clamp((i32)glm::round((f32)currentShellDamage + shellDamage), 0, 255);
        mShellDamageZones[slice] = newShellDamage;
        
        i32 appliedShellSamage = newShellDamage - currentShellDamage;

        // Apply health damage
        return i32v2((i32)glm::round(healthDamage) - appliedShellSamage, appliedShellSamage);
    };

    // TODO: Either remove weight, or use it
    weight1 = 1.0f;
    weight2 = 1.0f;

    // Apply weighted damage to both slices
    const i32v2 appliedDamage1 = applyDamageToSlice(slice1, weight1);
    const i32v2 appliedDamage2 = slice2 == slice1 ? i32v2(0) : applyDamageToSlice(slice2, weight2);

    // Calculate total applied damage
    const i32 maxAppliedHealthDamage = glm::max(appliedDamage1.x, appliedDamage2.x);
    if (maxAppliedHealthDamage >= mCurrentHealth) {
        mCurrentHealth = 0;
    }
    else {
        mCurrentHealth -= maxAppliedHealthDamage;
    }

    const i32 maxAppliedShellDamage = glm::max(appliedDamage1.y, appliedDamage2.y);
    LOG_DEBUG("DAMAGE APPLIED: {}  {}", maxAppliedHealthDamage, maxAppliedShellDamage);

    return maxAppliedHealthDamage;
}
