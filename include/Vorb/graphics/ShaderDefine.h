#pragma once

#include <vector>

struct ShaderDefine {
    nString name;
    bool active = true;
};

using ShaderDefinesVector = std::vector<ShaderDefine>;