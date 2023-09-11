#pragma once

// Enum cast
template<typename E>
constexpr auto e_cast(E e) -> typename std::underlying_type<E>::type {
    return static_cast<typename std::underlying_type<E>::type>(e);
}

#define e_count(e) (e_cast(e::COUNT))

#define KEG_ENUM_STR(e, v) kes_##e##.getValueFromKey(v).c_str()

template<typename E>
constexpr auto keg_enum_str(E e) -> const char * {
    return static_cast<typename std::underlying_type<E>::type>(e);
}

// Unused Parameter
#define UNUSED(x) (void)(x)


// Useful for determining the size of a class pre-compile time
//https://stackoverflow.com/questions/20979565/how-can-i-print-the-result-of-sizeof-at-compile-time-in-c
#define SIZER(type) char (*__kaboom)[sizeof(type)] = 1;

#define IS_ENABLED(d) d == 1

template<int M>
inline bool IsEnabled() {
    return true;
}

template<>
inline bool IsEnabled<0>() {
    return false;
}

constexpr f32 MS_PER_SECOND = 1000.0f;
constexpr f32 SECONDS_PER_MS = 0.001f;
constexpr f64 MS_PER_SECOND_D = 1000.0;
constexpr f64 SECONDS_PER_MS_D = 0.001;
constexpr float SECONDS_PER_DAY = 1440.0f;
constexpr float HOURS_PER_DAY = 24.0f;
constexpr float SECONDS_PER_HOUR = SECONDS_PER_DAY / HOURS_PER_DAY;

// Autoinit on startup
#define RUNTIME_INIT_FUNC(name) namespace { struct name { name (); } name##_ins; } name::name()
