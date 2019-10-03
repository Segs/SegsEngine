# Godot's own sources
file(GLOB_RECURSE source_files "*.cpp")
file(GLOB_RECURSE header_files "*.h")

foreach(tgt ${global_targets})
    target_sources(${tgt}_engine PRIVATE
        ${source_files} ${header_files}
        "${PROJECT_SOURCE_DIR}/thirdparty/misc/easing_equations.cpp"
        "${PROJECT_SOURCE_DIR}/thirdparty/misc/mikktspace.c"
    )
endforeach()
source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${source_files} ${header_files})
