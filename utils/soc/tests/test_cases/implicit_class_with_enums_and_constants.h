#pragma once

#include "core/reflection_macros.h"

#include <stdint.h>

class Bar {
    SE_CLASS()

    enum {
        CONSTV = 22,
        CONSTB = 23,
    };
    SE_CONSTANT(CONSTV)
    SE_CONSTANT(CONSTB)
    enum class AllThings {
        AreGood,
        AreBad = 22
    };
    SE_ENUM(AllThings)
    enum SmallStuff : uint8_t {
        Dwarf = 2,
        Pixie = 33
    };
    SE_ENUM(SmallStuff)
    enum class Important : int8_t {
        Negative = -1,
        Positive = 1
    };
    SE_ENUM(Important)

};

