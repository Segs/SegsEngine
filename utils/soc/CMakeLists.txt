find_package(Qt5 COMPONENTS Core REQUIRED)

add_library(soc_lib STATIC
    reflection_walker.cpp
    cpp_visitor.cpp
    cpp_visitor.h
    json_visitor.cpp
    json_visitor.h
    type_system.cpp
    type_system.h
    visitor_interface.h
)
target_link_libraries(soc_lib PUBLIC Qt5::Core )
target_include_directories(soc_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(soc_lib PROPERTIES FOLDER utils/soc/)

add_executable(reflection_walker
    soc_commandline.cpp
)
target_link_libraries(reflection_walker PRIVATE soc_lib)
set_target_properties(reflection_walker PROPERTIES FOLDER utils/soc/)

if(WIN32 AND NOT(CMAKE_GENERATOR MATCHES "Visual Studio"))
    #TODO: this is bonkers, we should be able to provide qt path to all code generator tools that run during build :|
    add_custom_command(TARGET reflection_walker POST_BUILD
        COMMAND cmake -E copy ${Qt5_BIN_DIR}/Qt5Cored.dll ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND cmake -E copy ${Qt5_BIN_DIR}/Qt5Core.dll ${CMAKE_CURRENT_BINARY_DIR})
endif()


add_subdirectory(tests)
# add selected Qt path to environment on visual studio
set_target_properties(reflection_walker PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=${CMAKE_MSVCIDE_RUN_PATH};%PATH%")

if(WIN32)
    install(TARGETS reflection_walker EXPORT SegsEngine
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION bin
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
    )
else()
    install(TARGETS reflection_walker EXPORT SegsEngine
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION bin
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
    )
endif()

