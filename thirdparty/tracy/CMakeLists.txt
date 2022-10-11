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
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp
    server/TracyBadVersion.cpp
    server/TracyColor.cpp
    server/TracyFilesystem.cpp
    server/TracyMemory.cpp
    server/TracyMicroArchitecture.cpp
    server/TracyMmap.cpp
    server/TracyMouse.cpp
    server/TracyPrint.cpp
    server/TracySourceView.cpp
    server/TracyStackFrames.cpp
    server/TracyStorage.cpp
    server/TracyTaskDispatch.cpp
    server/TracyTexture.cpp
    server/TracyTextureCompression.cpp
    server/TracyThreadCompress.cpp
    server/TracyUserData.cpp
    server/TracyView.cpp
    server/TracyWorker.cpp
#    zstd/debug.c
#    zstd/entropy_common.c
#    zstd/error_private.c
#    zstd/fse_compress.c
#    zstd/fse_decompress.c
#    zstd/hist.c
#    zstd/huf_compress.c
#    zstd/huf_decompress.c
#    zstd/pool.c
#    zstd/threading.c
#    zstd/xxhash.c
#    zstd/zstdmt_compress.c
#    zstd/zstd_common.c
#    zstd/zstd_compress.c
#    zstd/zstd_compress_literals.c
#    zstd/zstd_compress_sequences.c
#    zstd/zstd_compress_superblock.c
#    zstd/zstd_ddict.c
#    zstd/zstd_decompress.c
#    zstd/zstd_decompress_block.c
#    zstd/zstd_double_fast.c
#    zstd/zstd_fast.c
#    zstd/zstd_lazy.c
#    zstd/zstd_ldm.c
#    zstd/zstd_opt.c
    profiler/libs/gl3w/GL/gl3w.c
    profiler/src/HttpRequest.cpp
    profiler/src/imgui_freetype.cpp
    profiler/src/imgui_impl_glfw.cpp
    profiler/src/imgui_impl_opengl3.cpp
    profiler/src/main.cpp
    profiler/src/NativeWindow.cpp
    profiler/src/ResolvService.cpp

#    common/TracySocket.cpp
#    common/TracySystem.cpp
#    common/tracy_lz4.cpp
#    common/tracy_lz4hc.cpp
#    imgui/imgui.cpp
#    imgui/imgui_demo.cpp
#    imgui/imgui_draw.cpp
#    imgui/imgui_widgets.cpp
#    server/TracyBadVersion.cpp
#    server/TracyColor.cpp
#    server/TracyMemory.cpp
#    server/TracyMouse.cpp
#    server/TracyPrint.cpp
#    server/TracyStorage.cpp
#    server/TracySourceView.cpp
#    server/TracyStackFrames.cpp
#    server/TracyTaskDispatch.cpp
#    server/TracyTexture.cpp
#    server/TracyTextureCompression.cpp
#    server/TracyThreadCompress.cpp
#    server/TracyUserData.cpp
#    server/TracyView.cpp
#    server/TracyWorker.cpp
#    profiler/src/imgui_freetype.cpp
#    profiler/src/imgui_impl_glfw.cpp
#    profiler/src/imgui_impl_opengl3.cpp
#    profiler/src/ResolvService.cpp
#    profiler/src/NativeWindow.cpp
#    profiler/src/HttpRequest.cpp
#    profiler/libs/gl3w/GL/gl3w.c
#    profiler/src/main.cpp
    ${PLATFORM_SOURCES}
)
target_link_libraries(tracy_profiler PRIVATE freetype ${OPENGL_LIBRARIES} ${CMAKE_DL_LIBS} Qt5::Widgets Threads::Threads glfw zstd_internal)
if(TBB_LIB) # needed for libcpp + clang
    target_link_libraries(tracy_profiler PRIVATE ${TBB_LIB})
endif()

target_include_directories(tracy_profiler PRIVATE imgui profiler/libs/gl3w )
target_compile_definitions(tracy_profiler PRIVATE
    TRACY_FILESELECTOR
    TRACY_EXTENDED_FONT
    TRACY_ROOT_WINDOW
    IMGUI_IMPL_OPENGL_LOADER_GL3W
)
if(WIN32)
    target_compile_definitions(tracy_profiler PRIVATE NDEBUG _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE WIN32_LEAN_AND_MEAN NOMINMAX _USE_MATH_DEFINES)
    target_link_libraries(tracy_profiler PRIVATE ws2_32 dbghelp)
else()
    target_compile_definitions(tracy_profiler PRIVATE DISPLAY_SERVER_X11)
    target_link_libraries(tracy_profiler PRIVATE capstone)

endif()
