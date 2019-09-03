file(GLOB source_files "visual/*.cpp")
file(GLOB header_files "visual/*.h")
target_sources(${tgt}_servers PRIVATE ${source_files} ${header_files})
