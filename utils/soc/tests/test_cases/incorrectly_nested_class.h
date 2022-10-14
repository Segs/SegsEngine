#pragma once

#include "core/reflection_macros.h"
#include "core/object.h"
#include "core/reference.h"

#include <stdint.h>
class EditorSelection;
class Object;
template<class T>
class Ref;

class Bar : public Node {
    class Foo : Node2 {
        SE_CLASS()
        SE_INVOCABLE void test2();
    };
};



