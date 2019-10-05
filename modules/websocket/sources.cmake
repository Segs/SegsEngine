# This file is included from parent-dir CMakeLists.txt

file(GLOB_RECURSE source_files "${module_dir}/*.cpp")
file(GLOB_RECURSE header_files "${module_dir}/*.h")
file(GLOB_RECURSE qrc_files "${module_dir}/*.qrc")

list(APPEND module_sources ${source_files} ${header_files} ${qrc_files})
target_link_libraries(${tgt}_modules PRIVATE wslay)

