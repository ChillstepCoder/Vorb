#include "stdafx.h"
#include "MeshConst.h"

SERIALIZABLE_ENUM_SAME_NAME(MeshWindType,
    pair{ MeshWindType::None, "none"sv },
    pair{ MeshWindType::Grass, "grass"sv },
    pair{ MeshWindType::TreeTrunk, "tree_trunk"sv },
    pair{ MeshWindType::TreeLeaves, "tree_leaves"sv }
);
static_assert(e_count(MeshWindType) == 4, "Update yml definition");