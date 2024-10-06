// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/format.hh"

std::string util::format(const char *fmt, ...)
{
    va_list vaptr;

    va_start(vaptr, fmt);
    std::vector<char> buffer = {};
    buffer.resize(1 + std::vsnprintf(nullptr, 0, fmt, vaptr));
    va_end(vaptr);

    va_start(vaptr, fmt);
    std::vsnprintf(buffer.data(), buffer.size(), fmt, vaptr);
    va_end(vaptr);

    return std::string(buffer.data());
}

std::string util::vformat(const char *fmt, va_list vaptr)
{
    va_list vaptr_copy;
    va_copy(vaptr_copy, vaptr);

    std::vector<char> buffer = {};
    buffer.resize(1 + std::vsnprintf(nullptr, 0, fmt, vaptr));
    std::vsnprintf(buffer.data(), buffer.size(), fmt, vaptr_copy);

    va_end(vaptr_copy);

    return std::string(buffer.data());
}
