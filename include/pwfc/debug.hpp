#pragma once

#ifdef _DEBUG

#include <cstdio>

#define PWFC_LOG_INFO(...) do { \
    printf("[INFO] [%s:%s:%d] ", __FILE__, __FUNCTION__, __LINE__); \
    printf(__VA_ARGS__); \
    printf("\n"); \
    fflush(stdout); \
} while(0)
#define PWFC_ASSERT(x, ...) if(!(x)) do { \
	PWFC_LOG_ERROR(__VA_ARGS__); \
    abort();\
} while(0)

#else 

#define PWFC_ASSERT(x, ...)
#define PWFC_LOG_INFO(...)

#endif

#define PWFC_LOG_WARN(...) do { \
    printf("[WARNING] [%s:%d] ", __FILE__, __LINE__); \
    printf(__VA_ARGS___);\
    printf("\n");   \
    fflush(stdout); \
} while(0)
#define PWFC_LOG_ERROR(...) do { \
    printf("[ERROR] [%s:%d] ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); \
    printf("\n");\
    fflush(stdout);\
} while(0)
