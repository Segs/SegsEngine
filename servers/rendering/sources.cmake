file(GLOB source_files "rendering/*.cpp")
file(GLOB header_files "rendering/*.h")
target_sources(${tgt}_servers PRIVATE ${source_files} ${header_files})
