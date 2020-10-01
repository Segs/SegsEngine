# This directory contains all the utilities used during the project's build

# method binding header generator
#find_package(Qt5 COMPONENTS Core REQUIRED)
#add_executable(make_binders make_binders.cpp)
#target_link_libraries(make_binders Qt5::Core)

# shader to header converted
find_package(Qt5 COMPONENTS Core REQUIRED)
add_executable(shader_to_header shader_to_header.cpp)
target_link_libraries(shader_to_header Qt5::Core)
set_target_properties(shader_to_header PROPERTIES FOLDER utils)

# shader to header converted
find_package(Qt5 COMPONENTS Core REQUIRED)
add_executable(editor_to_header license_to_header.cpp)
target_link_libraries(editor_to_header Qt5::Core)
set_target_properties(editor_to_header PROPERTIES FOLDER utils)
if(WIN32 AND NOT(CMAKE_GENERATOR MATCHES "Visual Studio"))
    #TODO: this is bonkers, we should be able to provide qt path to all code generator tools that run during build :|
    add_custom_command(TARGET editor_to_header POST_BUILD
        COMMAND cmake -E copy ${Qt5_BIN_DIR}/Qt5Cored.dll ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND cmake -E copy ${Qt5_BIN_DIR}/Qt5Core.dll ${CMAKE_CURRENT_BINARY_DIR})
endif()
