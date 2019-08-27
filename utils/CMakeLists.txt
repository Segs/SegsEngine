# This directory contains all the utilities used during the project's build

# method binding header generator
find_package(Qt5 COMPONENTS Core REQUIRED)
add_executable(make_binders make_binders.cpp)
target_link_libraries(make_binders Qt5::Core)

# shader to header converted
find_package(Qt5 COMPONENTS Core REQUIRED)
add_executable(shader_to_header shader_to_header.cpp)
target_link_libraries(shader_to_header Qt5::Core)

# shader to header converted
find_package(Qt5 COMPONENTS Core REQUIRED)
add_executable(editor_to_header license_to_header.cpp)
target_link_libraries(editor_to_header Qt5::Core)
