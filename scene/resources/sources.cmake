file(GLOB source_files "resources/*.cpp")
file(GLOB header_files "resources/*.h")
target_sources(${tgt}_scene PRIVATE ${source_files} ${header_files})

file(GLOB source_files "resources/default_theme/*.cpp")
file(GLOB header_files "resources/default_theme/*.h")
target_sources(${tgt}_scene PRIVATE ${source_files} ${header_files})
