#include "stdafx.h"
#include "ThreadIDs.h"

#include <shared_mutex>

std::thread::id GAME_THREAD_ID = {};
std::thread::id NAV_THREAD_ID = {};
std::thread::id RENDER_THREAD_ID = {};
std::thread::id VISIBILITY_THREAD_ID = {};

std::unordered_map<std::thread::id, nString> sThreadNames;
static std::shared_mutex sThreadNameMutex;

extern void setThreadName(const char* name) {
    // Write access
    {
        std::lock_guard lock(sThreadNameMutex);
        sThreadNames[std::this_thread::get_id()] = name;
    }
    const size_t cSize = strlen(name) + 1;
    size_t outSize;
    wchar_t wc[64];
    mbstowcs_s(&outSize, wc, cSize, name, 64);
    SetThreadDescription(GetCurrentThread(), wc);
}

extern nString getThreadName(const std::thread::id& id) {
    // Read access
    std::shared_lock lock(sThreadNameMutex);
    auto&& it = sThreadNames.find(id);
    if (it == sThreadNames.end()) {
        return std::string("Worker ") + std::to_string(std::hash<std::thread::id>{}(id));
    }
    return it->second;
}
