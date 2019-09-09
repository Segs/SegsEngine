file(GLOB source_files "gridmap/*.cpp")
file(GLOB header_files "gridmap/*.h")

target_sources(${tgt}_modules PRIVATE ${source_files} ${header_files})
target_compile_definitions(${tgt}_modules PUBLIC MODULE_GRIDMAP_ENABLED)
