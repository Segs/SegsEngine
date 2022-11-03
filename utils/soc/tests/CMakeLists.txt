set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)

find_package(Qt5Test REQUIRED)

add_custom_target(copy-test-files ALL
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_CURRENT_BINARY_DIR}/test_cases
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/test_cases/ ${CMAKE_CURRENT_BINARY_DIR}/test_cases
)

add_executable(soc_namespace tst_namespace.cpp)
add_test(NAME soc_namespace COMMAND soc_namespace)
target_link_libraries(soc_namespace PRIVATE Qt5::Test soc_lib)
add_dependencies(soc_namespace copy-test-files)

set_target_properties(soc_namespace PROPERTIES FOLDER utils/soc/tests/)

# Test generating cpp reflection data and all ways of querying it.

macro(SE_WRAP_H target  ) # inputfiles
    if(TARGET reflection_walker)
        get_target_property(TGT_BINDIR ${target} BINARY_DIR)
        FILE(MAKE_DIRECTORY ${TGT_BINDIR}/soc)
        foreach(it ${ARGN}) # visit all paths
            file(GLOB headers CONFIGURE_DEPENDS ${it}/*.h)
            foreach(hdr ${headers}) # visit all paths
                message("soc -c ${hdr}")
                get_filename_component(name ${hdr} NAME_WE)
                get_filename_component(abs_source ${it} ABSOLUTE)

                message(${TGT_BINDIR}/soc/${name}_soc.cpp)
    #            set(out_file ${TGT_BINDIR})
                add_custom_command(
                    OUTPUT ${TGT_BINDIR}/soc/${name}_soc.cpp
                    COMMAND reflection_walker -c -o ${TGT_BINDIR}/soc ${hdr}
                    DEPENDS ${hdr} reflection_walker
                    VERBATIM
                )
                target_sources(${target} PRIVATE ${TGT_BINDIR}/soc/${name}_soc.cpp)
            endforeach()
        endforeach()
    else()
        message(ERROR " reflection walker target not found")
    endif()
endmacro()

set(SOURCES
    tst_runtime.cpp
)
add_executable(soc_runtime_test ${SOURCES})
SE_WRAP_H(soc_runtime_test dummy_runtime_classes)
get_target_property(TGT_SOURCES soc_runtime_test SOURCES)
message(TGT_SOURCES ${TGT_SOURCES})

add_test(NAME soc_runtime_test COMMAND soc_runtime_test)
target_link_libraries(soc_runtime_test PRIVATE Qt5::Test soc_lib)
target_link_libraries(soc_runtime_test PRIVATE se_core)

set_target_properties(soc_runtime_test PROPERTIES FOLDER utils/soc/tests/)
