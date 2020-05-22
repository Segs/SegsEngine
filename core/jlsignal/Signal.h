#pragma once

// Uncomment this to see verbose console messages about Signal/SignalObserver
// connections and disconnections. Recommended for debug only.
//#define JL_SIGNAL_ENABLE_LOGSPAM

// Uncomment this to force assertion failures when a SignalObserver tries
// to connect the same slot to the same signal twice. Recommended for debug only.
//#define JL_SIGNAL_ASSERT_ON_DOUBLE_CONNECT

/**
 * Quick usage guide:
 *
 * Use Signal to call an arbitrary number of functions with a single compile-time function call.
 * Currently, Signal only supports calls to object-method pairs, i.e., it is not possible to call
 * a static, non-instance function using Signal.
 *
 * There are three ways of declaring a signal:
 *
 * - Base signal class, with parameter count in the type name:
 *        Signal2< int, int > mySignal;
 *
 * - Template wrapper, using function signature syntax:
 *        Signal< void(int, int) > mySignal;
 *
 * - Template wrapper + macro (easiest, slightly hacky):
 *        JL_SIGNAL( int, int ) mySignal;
 */

#include "SignalDefinitions.h"
