#pragma once

#include "core/reflection_macros.h"
#include "core/object.h"
#include "core/reference.h"

#include <stdint.h>

class SignalSource {
    SE_CLASS()

SE_SIGNALS public:
    void null_signal();
    void single_arg(int arg1);
public:
};
