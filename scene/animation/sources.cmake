file(GLOB source_files "animation/*.cpp")
file(GLOB header_files "animation/*.h")
target_sources(${tgt}_scene PRIVATE ${source_files} ${header_files})
