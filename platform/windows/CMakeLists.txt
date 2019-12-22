#!/usr/bin/env python
find_package(OpenGL REQUIRED)

foreach(tgt ${global_targets})
    set(sources
        camera_win.cpp
        context_gl_windows.cpp
        crash_handler_windows.cpp
        key_mapping_windows.cpp
        joypad_windows.cpp
        power_windows.cpp
        windows_terminal_logger.cpp
        os_windows.cpp
    )
    target_sources(${tgt}_engine PRIVATE ${sources} )
    set(NO_UNITY_SOURCES ${sources} PARENT_SCOPE)

    #target_sources(segs_${tgt} PRIVATE "godot_res.rc")
    target_link_libraries(${tgt}_engine PRIVATE ${tgt}_modules ${tgt}_core ${LIBRARIES} ${OPENGL_LIBRARIES})

    target_compile_definitions(${tgt}_engine PRIVATE ${godot_${tgt}_DEFINES})
    target_include_directories(${tgt}_engine PRIVATE ${${PROJECT_BINARY_DIR}})
    set_common_target_properties(${tgt}_engine)

endforeach()
