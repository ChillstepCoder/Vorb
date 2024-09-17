#pragma once

// Thread stuff
extern std::thread::id GAME_THREAD_ID;
extern std::thread::id NAV_THREAD_ID;
extern std::thread::id RENDER_THREAD_ID;
extern std::thread::id VISIBILITY_THREAD_ID;
extern std::thread::id SIM_THREAD_ID;
extern std::thread::id GENERATION_THREAD_ID; // Transient thread for world generation

#define IS_GAME_THREAD() (std::this_thread::get_id() == GAME_THREAD_ID)
#define IS_NAV_THREAD() (std::this_thread::get_id() == NAV_THREAD_ID)
#define IS_RENDER_THREAD() (std::this_thread::get_id() == RENDER_THREAD_ID)
#define IS_VISIBILITY_THREAD() (std::this_thread::get_id() == VISIBILITY_THREAD_ID)
#define IS_SIM_THREAD() (std::this_thread::get_id() == SIM_THREAD_ID)
#define IS_GENERATION_THREAD() (std::this_thread::get_id() == GENERATION_THREAD_ID)

#define ASSERT_GAME_THREAD() (assert(IS_GAME_THREAD()))
#define ASSERT_NAV_THREAD() (assert(IS_NAV_THREAD()))
#define ASSERT_RENDER_THREAD() (assert(IS_RENDER_THREAD()))
#define ASSERT_VISIBILITY_THREAD() (assert(IS_VISIBILITY_THREAD()))
#define ASSERT_SIM_THREAD() (assert(IS_SIM_THREAD()))
#define ASSERT_GENERATION_THREAD() (assert(IS_GENERATION_THREAD()))

extern void setThreadName(const char* name);
extern nString getThreadName(const std::thread::id& id);
extern UnorderedFlatMap<std::thread::id, nString> sThreadNames;