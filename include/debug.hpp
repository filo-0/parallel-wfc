#pragma once

#ifdef _DEBUG

#include <print>
#define PWFC_LOG_INFO(...) do { \
    std::print("[INFO] [{}:{}:{}] ", __FILE__, __FUNCTION__, __LINE__); \
    std::println(__VA_ARGS__); \
    std::fflush(stdout); \
} while(0)
#define PWFC_LOG_WARN(...) do { std::print("[WARNING] [{}:{}:{}] ", __FILE__, __FUNCTION__, __LINE__); std::println(__VA_ARGS__); std::fflush(stdout); } while(0)
#define PWFC_LOG_ERROR(...) do { std::print("[ERROR] [{}:{}:{}] ", __FILE__, __FUNCTION__, __LINE__); std::println(__VA_ARGS__); std::fflush(stdout); } while(0)
#define PWFC_ASSERT(x, ...) if(!(x)) do { \
	PWFC_LOG_ERROR(__VA_ARGS__); \
} while(0)

#else 

#define PWFC_ASSERT(x, ...)
#define PWFC_LOG_INFO(...)
#define PWFC_LOG_WARN(...)
#define PWFC_LOG_ERROR(...)

#endif
