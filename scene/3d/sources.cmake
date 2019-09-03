file(GLOB source_files "3d/*.cpp")
file(GLOB header_files "3d/*.h")
target_sources(${tgt}_scene PRIVATE ${source_files} ${header_files})
