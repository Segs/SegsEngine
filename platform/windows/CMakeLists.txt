#!/usr/bin/env python
set(common_win 
#   "godot_res.rc"
	godot_windows.cpp
    camera_win.cpp
    context_gl_windows.cpp
    crash_handler_windows.cpp
    os_windows.cpp
    key_mapping_windows.cpp
    joypad_windows.cpp
    power_windows.cpp
    windows_terminal_logger.cpp
)

foreach(tgt ${global_targets})
    find_package(OpenGL REQUIRED)
    add_executable(godot_${tgt} ${common_win})
    target_link_libraries(godot_${tgt} ${tgt}_main editor ${tgt}_scene ${tgt}_servers)
    target_link_libraries(godot_${tgt} ${LIBRARIES} ${OPENGL_LIBRARIES})
    set_target_properties(godot_${tgt} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
    set_target_properties(godot_${tgt} PROPERTIES ENABLE_EXPORTS ON)
    target_compile_definitions(godot_${tgt} PRIVATE ${godot_${tgt}_DEFINES})
    target_include_directories(godot_${tgt} PRIVATE ${${PROJECT_BINARY_DIR}})
    target_compile_definitions(godot_${tgt} PRIVATE GODOT_EXPORTS)
endforeach()
