#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The shadow check's quick tier (tools/shadow/full.sh): every CV1000 set's check recipe, translated and interpreted, with
# the board's slowdown and without, and 3,000 frames at the first stage's boss; about half an hour a set on one core.
#   tools/shadow/quick.sh ROMS [OUT]   (the same environment as full.sh: JOBS, SETS, WORK_DIR, REF)
TIER=quick exec "$(dirname "$0")/full.sh" "$@"
