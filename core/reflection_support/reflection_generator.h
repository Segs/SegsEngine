#pragma once

struct ReflectionData;
class DocData;
enum class ReflectionSource {
    Core,
    Editor
};
void _initialize_reflection_data(ReflectionData& rd,ReflectionSource src);
