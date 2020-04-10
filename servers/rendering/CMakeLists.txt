file(GLOB source_files "*.cpp")
list(APPEND servers_sources ${source_files})
set(servers_sources ${servers_sources} PARENT_SCOPE)
