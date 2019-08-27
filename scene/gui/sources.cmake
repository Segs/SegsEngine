file(GLOB source_files "gui/*.cpp")
file(GLOB header_files "gui/*.h")
target_sources(${tgt}_scene PRIVATE ${source_files} ${header_files})
