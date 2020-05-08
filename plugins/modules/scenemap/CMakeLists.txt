# This file is included from parent-dir CMakeLists.txt

set(module_dir ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE source_files "${module_dir}/*.cpp")
file(GLOB_RECURSE header_files "${module_dir}/*.h")
file(GLOB_RECURSE qrc_files "${module_dir}/*.qrc")

set_plugin_options(
    NAME scenemap
    CLASSPROP SceneMapModule
    TYPE module
    SOURCES ${source_files} ${header_files} ${qrc_files}
)
