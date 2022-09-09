#pragma once

// Enum cast
template<typename E>
constexpr auto e_cast(E e) -> typename std::underlying_type<E>::type {
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


#define RUNTIME_INIT_FUNC(name) namespace { struct name { name (); } name##_ins; } name::name()