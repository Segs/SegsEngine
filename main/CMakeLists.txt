set(splash_data)
set(splash_editor_data)
set(app_icon_data)

BIN2HexArray(SOURCE_FILE "splash.png" TARGET_VAR splash_data)
configure_file("splash.h.cmake" "splash.gen.h")
BIN2HexArray(SOURCE_FILE "splash_editor.png" TARGET_VAR splash_editor_data)
configure_file("splash_editor.h.cmake" "splash_editor.gen.h")
BIN2HexArray(SOURCE_FILE "app_icon.png" TARGET_VAR app_icon_data)
configure_file("app_icon.h.cmake" "app_icon.gen.h")

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(main_sources)
add_subdirectory(tests)

# Godot's own sources
file(GLOB source_files "*.cpp")
file(GLOB headers_files "*.h")
list(APPEND main_sources ${source_files}
    ${CMAKE_CURRENT_BINARY_DIR}/splash.gen.h ${CMAKE_CURRENT_BINARY_DIR}/splash_editor.gen.h ${CMAKE_CURRENT_BINARY_DIR}/app_icon.gen.h
)

foreach(tgt ${global_targets})

    target_sources(${tgt}_engine PRIVATE ${main_sources})
    target_link_libraries(${tgt}_engine PRIVATE ${tgt}_tests ${tgt}_drivers Qt5::Core ${tgt}_interface)
endforeach()

INSTALL (
    FILES ${headers_files}
    DESTINATION include/SegsEngine/main
)
