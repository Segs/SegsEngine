add_subdirectory(gles3/shaders)

file(GLOB source_files "gles3/*.cpp")
file(GLOB header_files "gles3/*.h" "gles3/*.inc")

target_sources(${tgt}_drivers PRIVATE ${source_files} ${header_files})
source_group("gles3" FILES ${source_files} ${header_files})
