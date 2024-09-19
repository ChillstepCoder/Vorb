#include "stdafx.h"
#include "BiomeUniqueID.h"

SERIALIZABLE_ENUM_SAME_NAME(BiomeUniqueID,
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Ocean),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Plains),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Plains_B),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Plains_C),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Mountains),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Mountains_B),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Mountains_C),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Forest),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Forest_B),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Forest_C),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Hotsprings),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Hotsprings_B),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Hotsprings_C),
);
static_assert(e_count(BiomeUniqueID) == 13);