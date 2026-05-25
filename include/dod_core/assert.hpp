#pragma once

// dod_core assertion macro. Default backend uses <cassert>; users may define
// DOD_ASSERT before including any dod_core header to override (for example,
// to route to a logger or to forward to a game-specific crash handler).
//
// Behavior:
//   Debug builds (NDEBUG not set): asserts the condition; on failure, prints
//   the expression and message, then calls std::abort.
//   Release builds (NDEBUG set):   compiled out entirely (no overhead, no
//   diagnostic). Misuse becomes undefined behavior.
//
// Semantics match EnTT's ENTT_ASSERT: failures are programming bugs, not
// recoverable runtime conditions.

#ifndef DOD_ASSERT
#include <cassert>
#define DOD_ASSERT(cond, msg) assert((cond) && (msg))
#endif
