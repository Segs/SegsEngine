file(GLOB source_files "camera/*.cpp")
file(GLOB header_files "camera/*.h")
target_sources(${tgt}_servers PRIVATE ${source_files} ${header_files})
