#pragma once

#include "base/config.hpp"
#include "base/log.hpp"

#include <boost/stacktrace.hpp>

#include <cstdlib>

#ifdef CONFIG_IS_DEBUG

#define ASSERT(condition) \
    if(!(condition)) { \
        LOG_WARNING << "Assertion failed: " << #condition << '\n' << boost::stacktrace::stacktrace(); \
        std::exit(base::config::EXIT_ASSERT_FAILED); \
    }

#define ASSERT_SOFT(condition) \
    if(!(condition)) { \
        LOG_WARNING << "Soft assertion failed: " << #condition << '\n' << boost::stacktrace::stacktrace(); \
    }

#else
#define ASSERT(condition) \
    do { \
        if(!(condition)) { \
            LOG_DEBUG << "Assertion failed: " << #condition << '\n' << boost::stacktrace::stacktrace(); \
        } \
    } while(0)

#define ASSERT_SOFT(condition) \
    do { \
    } while(0)
#endif
