// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once

namespace logging
{
void init(void);
void deinit(void);
void info(const char *fmt, ...);
void warn(const char *fmt, ...);
void crit(const char *fmt, ...);
} // namespace logging
