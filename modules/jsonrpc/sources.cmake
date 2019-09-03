file(GLOB source_files "jsonrpc/*.cpp")
file(GLOB header_files "jsonrpc/*.h")

target_sources(${tgt}_modules PRIVATE ${source_files} ${header_files})
target_compile_definitions(${tgt}_modules PUBLIC MODULE_JSONRPC_ENABLED)
