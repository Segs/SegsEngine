file(GLOB source_files "csg/*.cpp")
file(GLOB header_files "csg/*.h")

target_sources(${tgt}_modules PRIVATE ${source_files} ${header_files})
target_compile_definitions(${tgt}_modules PUBLIC MODULE_CSG_ENABLED)
