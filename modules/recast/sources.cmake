file(GLOB source_files "recast/*.cpp")
file(GLOB header_files "recast/*.h")

target_sources(${tgt}_modules PRIVATE ${source_files} ${header_files} ${recast_source_files})
target_link_libraries(${tgt}_modules PRIVATE recast)
target_compile_definitions(${tgt}_modules PUBLIC MODULE_RECAST_ENABLED RECAST_ENABLED)
