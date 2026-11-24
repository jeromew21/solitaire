// platform.h
#pragma once

// --- Operating System Detection ---
#if defined(__EMSCRIPTEN__)
    #define ANVIL_PLATFORM_WEB 1
#else
    #define ANVIL_PLATFORM_WEB 0
#endif

#if defined(_WIN32) // Covers both 32-bit and 64-bit Windows
    #define ANVIL_PLATFORM_WINDOWS 1
#else
    #define ANVIL_PLATFORM_WINDOWS 0
#endif

#if defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
        #define ANVIL_PLATFORM_IOS 1
        #define ANVIL_PLATFORM_MACOS 0
    #else
        #define ANVIL_PLATFORM_IOS 0
        #define ANVIL_PLATFORM_MACOS 1
    #endif
#else
    #define ANVIL_PLATFORM_IOS 0
    #define ANVIL_PLATFORM_MACOS 0
#endif

#if defined(__ANDROID__)
    #define ANVIL_PLATFORM_ANDROID 1
    #define ANVIL_PLATFORM_LINUX 0
#elif defined(__linux__) && !ANVIL_PLATFORM_WEB
    #define ANVIL_PLATFORM_ANDROID 0
    #define ANVIL_PLATFORM_LINUX 1
#else
    #define ANVIL_PLATFORM_ANDROID 0
    #define ANVIL_PLATFORM_LINUX 0
#endif

// --- Convenience Groupings ---
#define ANVIL_PLATFORM_DESKTOP (ANVIL_PLATFORM_WINDOWS || ANVIL_PLATFORM_MACOS || ANVIL_PLATFORM_LINUX)
#define ANVIL_PLATFORM_MOBILE  (ANVIL_PLATFORM_IOS || ANVIL_PLATFORM_ANDROID)
#define ANVIL_PLATFORM_POSIX   (ANVIL_PLATFORM_MACOS || ANVIL_PLATFORM_LINUX || ANVIL_PLATFORM_IOS || ANVIL_PLATFORM_ANDROID)

// --- Architecture Detection ---
#if defined(__aarch64__) || defined(_M_ARM64)
    #define ARCH_ARM64 1
#else
    #define ARCH_ARM64 0
#endif

#if defined(__x86_64__) || defined(_M_X64)
    #define ARCH_X64 1
#else
    #define ARCH_X64 0
#endif

// Compile-time sanity check so you never compile for an unknown/broken target
static_assert(
    ANVIL_PLATFORM_WINDOWS + ANVIL_PLATFORM_MACOS + ANVIL_PLATFORM_LINUX + ANVIL_PLATFORM_IOS + ANVIL_PLATFORM_ANDROID + ANVIL_PLATFORM_WEB == 1,
    "Exactly one target platform must be active!"
);
