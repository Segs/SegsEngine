# Godot source files
file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

if(OPTION_BUILTIN_SQUISH)
# Thirdparty source files
set(thirdparty_sources
    ${PROJECT_SOURCE_DIR}/thirdparty/squish/alpha.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/squish/clusterfit.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/squish/colourblock.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/squish/colourfit.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/squish/colourset.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/squish/maths.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/squish/rangefit.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/squish/singlecolourfit.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/squish/squish.cpp

)
endif()

foreach(tgt ${global_targets})
    add_library(${tgt}_squish OBJECT ${source_files} ${header_files} ${thirdparty_sources})

    if(OPTION_BUILTIN_SQUISH)
        target_include_directories(${tgt}_squish PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/squish/)
    else()
        target_link_libraries(${tgt}_squish PUBLIC squish)
    endif()

    target_link_libraries(${tgt}_squish PUBLIC ${tgt}_core)
    set_common_target_properties(${tgt}_squish)
endforeach()
