#include "Vorb/stdafx.h"
#include "Vorb/io/FileOps.h"

#include "Vorb/io/filesystem.h"

bool vorb::io::buildDirectoryTree(const Path& path, bool omitEnd /*= false*/) {
    if (path.isNull()) return false;
    
    Path pa = path;
    if (omitEnd) {
        pa.trimEnd();
        if (pa.isNull()) return true; // Code successfully did nothing
    }

    fs::path bp(pa.getString());
    if (fs::exists(bp)) return true;
    return fs::create_directories(bp);
}

bool vorb::io::containsSubpath(const Path& path, const char* subPath) {
    const nString& pathStr = path.getString();
    int j = 0;
    for (size_t c = 0; c < pathStr.size(); ++c) {
        if (subPath[j] == '\0') {
            return true;
        }
        if (pathStr[c] == subPath[j]) {
            ++j;
        }
        else {
            j = 0;
        }
    }
    return false;
}

std::string vorb::io::getLeafNameFromFilePathNoExtension(const vio::Path& path) {
    std::string textureName = path.getLeaf();
    size_t i = textureName.size();
    bool hadExtension = false;
    for (; i > 0; --i) {
        if (textureName[i] == '.') {
            hadExtension = true;
            break;
        }
    }
    if (hadExtension) {
        textureName.resize(i); // Chop off .png
    }
    return textureName;
}
