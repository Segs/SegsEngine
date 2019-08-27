file(GLOB source_files "arvr/*.cpp")
file(GLOB header_files "arvr/*.h")
target_sources(${tgt}_servers PRIVATE ${source_files} ${header_files})
