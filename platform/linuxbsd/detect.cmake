if(UNIX)
    set(name "X11")
    SET(OPTION_DEBUG_SYMBOLS "no" CACHE STRING "debug symbol level")

    ## Build type

    # -O3 -ffast-math is identical to -Ofast. We need to split it out so we can selectively disable
    # -ffast-math in code for which it generates wrong results.
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # clang lowers some math functions to removed from libm _finite version in fast-math mode

        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ffast-math -fno-finite-math-only")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -ffast-math -fno-finite-math-only")
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ffast-math")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -ffast-math")
    endif()
    if (${OPTION_DEBUG_SYMBOLS} EQUAL "yes")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -g1")
    endif()
    if (${OPTION_DEBUG_SYMBOLS} EQUAL "full")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -g2")
    endif()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3")

    ## Architecture
    add_definitions(-DTYPED_METHOD_BIND)
    ## Dependencies
    find_package(X11)
    if(OPTION_TOUCH)
        if(NOT X11_Xi_FOUND)
            message(ERROR "When touch is enabled Xi library is required")
        else()
            add_definitions(-DTOUCH_ENABLED)
        endif()
    endif()


    # Sound and video libraries
    # Keep the order as it triggers chained dependencies (ogg needed by others, etc.)
    if(NOT OPTION_BUILTIN_LIBTHEORA)
        SET(OPTION_BUILTIN_LIBOGG OFF CACHE BOOL "Overriden" FORCE)
        SET(OPTION_BUILTIN_VORBIS OFF CACHE BOOL "Overriden" FORCE)
    endif()
    if(NOT OPTION_BUILTIN_LIBVPX)
        find_package(VPX)
    endif()
    if(NOT OPTION_BUILTIN_LIBVORBIS)
        SET(OPTION_BUILTIN_LIBOGG OFF CACHE BOOL "Overriden" FORCE)
        #find_package(vorbis REQUIRED)
        #find_package(vorbis_file REQUIRED)
    endif()

    if(NOT OPTION_BUILTIN_OPUS)
        SET(OPTION_BUILTIN_LIBOGG OFF CACHE BOOL "Overriden" FORCE) # Needed to link against system opus
        #find_package(Opus REQUIRED)
    endif()

    if(NOT OPTION_builtin_libogg)
        #find_package(Ogg REQUIRED)
    endif()

    if(OPTION_BUILTIN_LIBTHEORA)
        #list_of_x86 = ['x86_64', 'x86', 'i386', 'i586']
        #if any(platform.machine() in s for s in list_of_x86):
        #    env["x86_libtheora_opt_gcc"] = True
    endif()

    if(NOT OPTION_BUILTIN_PCRE2)
        find_package(PCRE REQUIRED) #env.ParseConfig('pkg-config libpcre2-32 --cflags --libs')
    endif()

    ## Flags

    if(UNIX AND NOT APPLE) # or use CMAKE_SYSTEM_NAME
        add_definitions(-DJOYDEV_ENABLED)
        if(OPTION_UDEV)
            find_package(Udev)
            if (UDEV_FOUND) # 0 means found
                message("Enabling udev support")
                target_compile_definitions(common_interface INTERFACE UDEV_ENABLED)
                target_include_directories(common_interface INTERFACE ${UDEV_INCLUDE_DIRS})
                target_link_libraries(common_interface INTERFACE ${UDEV_LIBRARIES})
            else()
                message("libudev development libraries not found, disabling udev support")
            endif()
        endif()
    endif()

    # Linkflags below this line should typically stay the last ones
    if(NOT OPTION_BUILTIN_ZLIB)
        find_package(ZLIB REQUIRED)
    endif()
    include_directories(${PROJECT_SOURCE_DIR}/platform/linuxbsd)
    add_definitions(-DX11_ENABLED -DUNIX_ENABLED -DOPENGL_ENABLED)

    ## Cross-compilation

    # Link those statically for portability
    if(OPTION_USE_STATIC_CPP)
        add_definitions(-static-libgcc -static-libstdc++)
    endif()
    set(can_build TRUE)
    list(APPEND platform_list linuxbsd)
endif()
