file(GLOB source_files "audio/effects/*.cpp")
file(GLOB header_files "audio/effects/*.h")
target_sources(${tgt}_servers PRIVATE ${source_files} ${header_files})

file(GLOB source_files "audio/*.cpp")
file(GLOB header_files "audio/*.h")
target_sources(${tgt}_servers PRIVATE ${source_files} ${header_files})
