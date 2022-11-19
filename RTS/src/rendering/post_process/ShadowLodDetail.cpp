#include "stdafx.h"
#include "ShadowLodDetail.h"

KEG_ENUM_DEF(ShadowLodDetail, ShadowLodDetail, kt) {
    kt.addValue("none", ShadowLodDetail::None);
    kt.addValue("low", ShadowLodDetail::Low);
    kt.addValue("medium", ShadowLodDetail::Medium);
    kt.addValue("high", ShadowLodDetail::High);
    kt.addValue("highest", ShadowLodDetail::Highest);
}
static_assert(e_cast(ShadowLodDetail::Highest) == 3, "Update keg definition");
