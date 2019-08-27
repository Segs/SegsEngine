file(GLOB source_files "2d/*.cpp")
file(GLOB header_files "2d/*.h")
target_sources(${tgt}_scene PRIVATE ${source_files} ${header_files})
