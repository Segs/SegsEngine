set(splash_data)
set(splash_editor_data)
set(app_icon_data)

BIN2HexArray(SOURCE_FILE "splash.png" TARGET_VAR splash_data)
configure_file("splash.h.cmake" "splash.gen.h")
BIN2HexArray(SOURCE_FILE "splash_editor.png" TARGET_VAR splash_editor_data)
configure_file("splash_editor.h.cmake" "splash_editor.gen.h")
BIN2HexArray(SOURCE_FILE "app_icon.png" TARGET_VAR app_icon_data)
configure_file("app_icon.h.cmake" "app_icon.gen.h")

file(GLOB controller_databases ${CMAKE_CURRENT_SOURCE_DIR} "gamecontroller_db/*.txt" )
add_custom_command(
    COMMAND editor_to_header controllers ${CMAKE_CURRENT_BINARY_DIR}/default_controller_mappings.gen.cpp ${controller_databases}
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/default_controller_mappings.gen.cpp
    DEPENDS editor_to_header ${controller_databases}
    COMMENT "Generating controllers header"
)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(main_sources)
add_subdirectory(tests)

# Godot's own sources
file(GLOB source_files "*.cpp")
list(APPEND main_sources ${source_files})
list(APPEND main_sources splash.gen.h splash_editor.gen.h app_icon.gen.h)
list(APPEND main_sources ${CMAKE_CURRENT_BINARY_DIR}/default_controller_mappings.gen.cpp)


foreach(tgt ${global_targets})
    add_library(${tgt}_main ${main_sources})
    target_link_libraries(${tgt}_main PUBLIC ${tgt}_tests ${tgt}_core ${tgt}_drivers Qt5::Core)
    if(OPTION_TOOLS)
        target_link_libraries(${tgt}_main PUBLIC edit_tools)
    endif()
    set_common_target_properties(${tgt}_main)
endforeach()

