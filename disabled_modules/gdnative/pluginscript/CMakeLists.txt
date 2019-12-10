file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )
set(sources ${sources} ${source_files} ${header_files} PARENT_SCOPE)
