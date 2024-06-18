#include "stdafx.h"
#include "TileDamageData.h"

POOLED_ALLOC_DEF_THREADSAFE(TileDamageData, 2048);

i32 TileDamageData::applyDamageStrike(f32v2 strikeNormal, f32 strikeRelativeZ, i32 baseStrikeDamage, TileDamageResistances tileResistance) {
    // TODO: VERTICAL
    UNUSED(strikeRelativeZ);
    // Determine shell slice
    const float angle = atan2f(strikeNormal.y, strikeNormal.x) + M_PIF;
    int slice = int(floor(angle / (M_2_PIF / MAX_DAMAGE_ZONE_RADIAL_SECTORS)));
    slice = std::clamp(slice, 0, MAX_DAMAGE_ZONE_RADIAL_SECTORS - 1);

    // Apply damage
    const ui8 currentShellDamage = mShellDamageZones[slice];
    const f32 currentSliceDamageRatio = currentShellDamage / 255.0f;
    const f32 currentResistance = lerp(tileResistance.outer, tileResistance.inner, powf(currentSliceDamageRatio, tileResistance.blendExponent));
    const f32 healthDamage = glm::max(baseStrikeDamage - currentResistance, 0.0f);
    const f32 shellDamage = healthDamage * tileResistance.shellDegradeMultiplier;


    // Apply shell damage
    ui8 newShellDamage = (ui8)glm::clamp((i32)glm::round((f32)currentShellDamage + shellDamage), 0, 255);
    mShellDamageZones[slice] = newShellDamage;

    // Apply health damage
    i32 appliedDamage = (i32)glm::round(healthDamage);
    if (appliedDamage > mCurrentHealth) {
        mCurrentHealth = 0;
    }
    else {
        mCurrentHealth -= appliedDamage;
    }
    return appliedDamage;
}
