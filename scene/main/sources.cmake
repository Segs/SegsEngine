file(GLOB source_files "main/*.cpp")
file(GLOB header_files "main/*.h")
target_sources(${tgt}_scene PRIVATE ${source_files} ${header_files})
