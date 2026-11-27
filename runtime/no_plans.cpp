// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// A game with no plans compiled into its translated code (runtime/plans.h): every event the generic path's
#include "plans.h"

namespace spelunk {

const PlanTables &Plans() {
    static const PlanTables none;
    return none;
}

} // namespace spelunk
