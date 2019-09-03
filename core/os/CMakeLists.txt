file(GLOB source_files "*.cpp")
file(GLOB header_files "*.h")
list(APPEND core_sources ${source_files} ${header_files})
set(core_sources ${core_sources} PARENT_SCOPE)
