file(GLOB source_files "*.cpp")
list(APPEND drivers_sources ${source_files})
set(drivers_sources ${drivers_sources} PARENT_SCOPE)

