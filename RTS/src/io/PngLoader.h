#pragma once

#include <gli/texture2d.hpp>
#include "filesystem/FileSystem.h"

class PngLoader
{
public:
    static gli::texture2d loadPng(const fs::path& path, bool flipV);
};

