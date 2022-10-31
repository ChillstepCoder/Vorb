#pragma once

// Thread stuff
extern std::thread::id GAME_THREAD_ID;
extern std::thread::id NAV_THREAD_ID;
extern std::thread::id RENDER_THREAD_ID;

#define IS_GAME_THREAD() (std::this_thread::get_id() == GAME_THREAD_ID)
#define IS_NAV_THREAD() (std::this_thread::get_id() == NAV_THREAD_ID)
#define IS_RENDER_THREAD() (std::this_thread::get_id() == RENDER_THREAD_ID)

extern void setThreadName(const char* name);
extern nString getThreadName(const std::thread::id& id);
extern std::unordered_map<std::thread::id, nString> sThreadNames;