file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

set(thirdparty_sources
    ${PROJECT_SOURCE_DIR}/thirdparty/cvtt/ConvectionKernels.cpp
)

foreach(tgt ${global_targets})
    add_library(${tgt}_cvtt OBJECT ${source_files} ${header_files} ${thirdparty_sources})
    target_include_directories(${tgt}_cvtt PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/cvtt/)
    target_link_libraries(${tgt}_cvtt PUBLIC ${tgt}_core)
    set_common_target_properties(${tgt}_cvtt)
endforeach()
