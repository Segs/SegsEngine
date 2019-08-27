file(GLOB source_files "*.cpp")
file(GLOB header_files "*.h")

target_sources(${tgt}_drivers PRIVATE ${source_files} ${header_files})
