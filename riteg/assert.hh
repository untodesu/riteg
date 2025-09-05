#pragma once

#define riteg_force_assert(condition)                                                             \
    do {                                                                                          \
        if(!(condition)) {                                                                        \
            LOG_CRITICAL("assertion failed: {}", #condition);                                     \
            LOG_CRITICAL("{}:{}", std::filesystem::path(__FILE__).filename().string(), __LINE__); \
            std::terminate();                                                                     \
        }                                                                                         \
    } while(false)

#define riteg_force_assert_msg(condition, message)                                                \
    do {                                                                                          \
        if(!(condition)) {                                                                        \
            LOG_CRITICAL("assertion failed: {}; {}", #condition, (message));                      \
            LOG_CRITICAL("{}:{}", std::filesystem::path(__FILE__).filename().string(), __LINE__); \
            std::terminate();                                                                     \
        }                                                                                         \
    } while(false)
