// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/logging.hh"

constexpr static const char *CONSOLE_INFO = "\x1B[1;32m[INFO]\x1B[0m ";
constexpr static const char *CONSOLE_WARN = "\x1B[1;33m[WARN]\x1B[0m ";
constexpr static const char *CONSOLE_CRIT = "\x1B[1;31m[CRIT]\x1B[0m ";

constexpr static const char *LOGFILE_INFO = "[INFO] ";
constexpr static const char *LOGFILE_WARN = "[WARN] ";
constexpr static const char *LOGFILE_CRIT = "[CRIT] ";

static std::ofstream riteg_log = {};

static void append_log(const char *prefix, const std::string &message)
{
}

void logging::init(void)
{
    riteg_log.open("riteg.log", std::ios::app);
}

void logging::deinit(void)
{
    riteg_log.close();
}

void logging::info(const char *fmt, ...)
{
    va_list vaptr;
    std::string msg;

    va_start(vaptr, fmt);
    msg.resize((1 + std::vsnprintf(nullptr, 0, fmt, vaptr)), char(0x00));
    va_end(vaptr);

    va_start(vaptr, fmt);
    std::vsnprintf(msg.data(), msg.size(), fmt, vaptr);
    va_end(vaptr);

    if(!std::cerr.bad()) std::cerr << CONSOLE_INFO << msg.c_str() << std::endl;
    if(!riteg_log.bad()) riteg_log << LOGFILE_INFO << msg.c_str() << std::endl;
}

void logging::warn(const char *fmt, ...)
{
    va_list vaptr;
    std::string msg;

    va_start(vaptr, fmt);
    msg.resize((1 + std::vsnprintf(nullptr, 0, fmt, vaptr)), char(0x00));
    va_end(vaptr);

    va_start(vaptr, fmt);
    std::vsnprintf(msg.data(), msg.size(), fmt, vaptr);
    va_end(vaptr);

    if(!std::cerr.bad()) std::cerr << CONSOLE_WARN << msg.c_str() << std::endl;
    if(!riteg_log.bad()) riteg_log << LOGFILE_WARN << msg.c_str() << std::endl;
}

void logging::crit(const char *fmt, ...)
{
    va_list vaptr;
    std::string msg;

    va_start(vaptr, fmt);
    msg.resize((1 + std::vsnprintf(nullptr, 0, fmt, vaptr)), char(0x00));
    va_end(vaptr);

    va_start(vaptr, fmt);
    std::vsnprintf(msg.data(), msg.size(), fmt, vaptr);
    va_end(vaptr);

    if(!std::cerr.bad()) std::cerr << CONSOLE_CRIT << msg.c_str() << std::endl;
    if(!riteg_log.bad()) riteg_log << LOGFILE_CRIT << msg.c_str() << std::endl;
}
