file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

foreach(tgt ${global_targets})
    if(OPTION_TOOLS)
        list(APPEND source_files editor/gdscript_highlighter.cpp editor/gdscript_highlighter.h)
    endif()

    add_library(${tgt}_gdscript OBJECT ${source_files} ${header_files})

    target_link_libraries(${tgt}_gdscript PUBLIC ${tgt}_core)
    set_common_target_properties(${tgt}_gdscript)
endforeach()
