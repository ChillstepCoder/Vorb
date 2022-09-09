#pragma once

// Thread stuff
const std::thread::id MAIN_THREAD_ID = std::this_thread::get_id();
extern std::thread::id NAV_THREAD_ID;

#define IS_MAIN_THREAD() (std::this_thread::get_id() == MAIN_THREAD_ID)
#define IS_NAV_THREAD() (std::this_thread::get_id() == NAV_THREAD_ID)