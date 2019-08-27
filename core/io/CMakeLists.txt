file(GLOB source_files "*.cpp")
list(APPEND core_sources ${source_files})
set(core_sources ${core_sources} PARENT_SCOPE)
