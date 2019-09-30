# Targeted Windows version: 7 (and later), minimum supported version
# XP support dropped after EOL due to missing API for IPv6 and other issues
# Vista support dropped after EOL due to GH-10243
set(target_win_version 0x0601 CACHE STRING "Targeted Windows version, >= 0x0601 (Windows 7)" FORCE)
set(name Windows)
if(MSVC)
    add_definitions(-DWINDOWS_ENABLED -DOPENGL_ENABLED -DRTAUDIO_ENABLED -DWASAPI_ENABLED -DTYPED_METHOD_BIND -DWIN32 -DMSVC -DWINVER=${target_win_version} -D_WIN32_WINNT=${target_win_version})
    set(LIBRARIES winmm opengl32 dsound kernel32 ole32 oleaut32
            user32 gdi32 IPHLPAPI Shlwapi wsock32 Ws2_32
            shell32 advapi32 dinput8 dxguid imm32 bcrypt Avrt)
	list(APPEND platform_list windows)
	include_directories(${PROJECT_SOURCE_DIR}/platform/windows)
endif()
if(MINGW)
    ## Build type

    ## Compile flags

    ADD_DEFINITIONS(-DWINDOWS_ENABLED -mwindows)
    ADD_DEFINITIONS(-DOPENGL_ENABLED)
    ADD_DEFINITIONS(-DRTAUDIO_ENABLED)
    ADD_DEFINITIONS(-DWASAPI_ENABLED)
    ADD_DEFINITIONS(-DWINVER=${target_win_version} -D_WIN32_WINNT=${target_win_version})
    set(LIBRARIES mingw32 opengl32 dsound ole32 d3d9 winmm gdi32 iphlpapi shlwapi wsock32 ws2_32 kernel32 oleaut32 dinput8 dxguid ksuser imm32 bcrypt Avrt)

    ADD_DEFINITIONS(-DMINGW_ENABLED)
	list(APPEND platform_list windows)
	include_directories(${PROJECT_SOURCE_DIR}/platform/windows)
endif()
