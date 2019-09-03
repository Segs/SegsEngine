set(common_x11
    context_gl_x11.cpp
    crash_handler_x11.cpp
    os_x11.cpp
    key_mapping_x11.cpp
    joypad_linux.cpp
    power_x11.cpp
    detect_prime.cpp
)

find_package(OpenGL REQUIRED)

foreach(tgt ${global_targets})
    add_executable(godot_${tgt} godot_x11.cpp ${common_x11})
    target_link_libraries(godot_${tgt} ${tgt}_main editor ${tgt}_scene ${tgt}_servers ${tgt}_modules ${tgt}_core)
    target_link_libraries(godot_${tgt} ${LIBRARIES} ${X11_LIBRARIES} ${X11_Xcursor_LIB} ${X11_Xrender_LIB} Xinerama Xrandr X11 dl pthread Xi ${OPENGL_LIBRARIES})
    set_target_properties(godot_${tgt} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
    set_target_properties(godot_${tgt} PROPERTIES ENABLE_EXPORTS ON)
    target_compile_definitions(godot_${tgt} PRIVATE ${godot_${tgt}_DEFINES})
    target_include_directories(godot_${tgt} PRIVATE ${${PROJECT_BINARY_DIR}})
endforeach()
