set(common_x11
    context_gl_x11.cpp
    crash_handler_x11.cpp
    os_x11.cpp
)

find_package(OpenGL REQUIRED)

foreach(tgt ${global_targets})
    target_sources(${tgt}_engine PRIVATE
        key_mapping_x11.cpp
        joypad_linux.cpp
        power_x11.cpp
        detect_prime.cpp
        context_gl_x11.cpp
        crash_handler_x11.cpp
        os_x11.cpp
    )
    target_link_libraries(${tgt}_engine PRIVATE
        ${tgt}_modules ${tgt}_core
        ${X11_LIBRARIES} ${X11_Xcursor_LIB} ${X11_Xrender_LIB} Xinerama Xrandr X11 dl pthread Xi ${OPENGL_LIBRARIES}
    )

    target_compile_definitions(${tgt}_engine PRIVATE ${godot_${tgt}_DEFINES})
    target_include_directories(${tgt}_engine PRIVATE ${${PROJECT_BINARY_DIR}})
    set_common_target_properties(${tgt}_engine)
endforeach()
