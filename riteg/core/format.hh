// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once

namespace util
{
std::string format(const char *fmt, ...);
std::string vformat(const char *fmt, va_list ap);
} // namespace util
