file(GLOB source_files "physics/joints/*.cpp")
file(GLOB header_files "physics/joints/*.h")
target_sources(${tgt}_servers PRIVATE ${source_files} ${header_files})

file(GLOB source_files "physics/*.cpp")
file(GLOB header_files "physics/*.h")
target_sources(${tgt}_servers PRIVATE ${source_files} ${header_files})
