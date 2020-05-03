
find_package(OpenGL REQUIRED)

foreach(tgt ${global_targets})
    set(sources
        ${CMAKE_CURRENT_SOURCE_DIR}/key_mapping_x11.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/joypad_linux.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/detect_prime.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/context_gl_x11.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/crash_handler_x11.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/os_x11.cpp
    )
    target_sources(${tgt}_engine PRIVATE
        ${sources}
    )
    set(NO_UNITY_SOURCES ${sources} PARENT_SCOPE)

    target_link_libraries(${tgt}_engine PRIVATE
        ${tgt}_modules ${tgt}_core
        ${X11_LIBRARIES} ${X11_Xcursor_LIB} ${X11_Xrender_LIB} Xinerama Xrandr X11 dl pthread Xi ${OPENGL_LIBRARIES}
    )

    target_compile_definitions(${tgt}_engine PRIVATE ${godot_${tgt}_DEFINES})
    target_include_directories(${tgt}_engine PRIVATE ${${PROJECT_BINARY_DIR}})
    set_common_target_properties(${tgt}_engine)
endforeach()
