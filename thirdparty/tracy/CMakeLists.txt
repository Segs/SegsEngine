find_package(OpenGL REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(Qt5 COMPONENTS Core Widgets REQUIRED)

find_library(TBB_LIB tbb)

#find_package(GLFW3)
if(WIN32)
    set(PLATFORM_SOURCES
		profiler/src/winmain.cpp
		profiler/src/winmainArchDiscovery.cpp
    )
else()
    set(PLATFORM_SOURCES
    )
endif()
add_executable(tracy_profiler
    common/TracySocket.cpp
    common/TracySystem.cpp
    common/tracy_lz4.cpp
    common/tracy_lz4hc.cpp
    imguicolortextedit/TextEditor.cpp
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp
    server/TracyBadVersion.cpp
    server/TracyMemory.cpp
    server/TracyPrint.cpp
    server/TracyStorage.cpp
    server/TracyTaskDispatch.cpp
    server/TracyTexture.cpp
    server/TracyThreadCompress.cpp
    server/TracyUserData.cpp
    server/TracyView.cpp
    server/TracyWorker.cpp
    profiler/src/imgui_freetype.cpp
    profiler/src/imgui_impl_glfw.cpp
    profiler/src/imgui_impl_opengl3.cpp
    profiler/src/ResolvService.cpp
    profiler/libs/gl3w/GL/gl3w.c
	profiler/src/main.cpp
    ${PLATFORM_SOURCES}
)
target_link_libraries(tracy_profiler PRIVATE freetype ${OPENGL_LIBRARIES} ${CMAKE_DL_LIBS} Qt5::Widgets Threads::Threads glfw)
if(TBB_LIB) # needed for libcpp + clang
    target_link_libraries(tracy_profiler PRIVATE ${TBB_LIB})
endif()

target_include_directories(tracy_profiler PRIVATE imgui profiler/libs/gl3w )
target_compile_definitions(tracy_profiler PRIVATE TRACY_FILESELECTOR TRACY_EXTENDED_FONT TRACY_ROOT_WINDOW IMGUI_IMPL_OPENGL_LOADER_GL3W)
if(WIN32)
	target_compile_definitions(tracy_profiler PRIVATE NDEBUG _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE WIN32_LEAN_AND_MEAN NOMINMAX _USE_MATH_DEFINES)
    target_link_libraries(tracy_profiler PRIVATE ws2_32 dbghelp)
endif()
