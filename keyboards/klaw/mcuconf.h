// Copyright 2026 Scramble Tools
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include_next <mcuconf.h>

// PWM slice 4 serves GP9, the buzzer pin (B5 on the Pro Micro footprint)
#undef RP_PWM_USE_PWM4
#define RP_PWM_USE_PWM4 TRUE
