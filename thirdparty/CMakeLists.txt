
set(ThirdParty_Install_Dir ${PROJECT_BINARY_DIR}/ThirdParty_Builds)
include(ExternalProject_EASTL.cmake)

macro(set_3rdparty_properties name)
    set_property(TARGET ${name} PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_common_target_properties(${name})
    set_target_properties(${name} PROPERTIES FOLDER 3rd_party)
endmacro()

add_subdirectory(entt)
add_subdirectory(bullet)
add_subdirectory(zlib)
add_subdirectory(embree)



add_library(thirdparty_misc STATIC
    # Add required thirdparty code. Header paths are hardcoded, we don't need to append
    # to the include path (saves a few chars on the compiler invocation for touchy MSVC...)
        # C sources
        ${PROJECT_SOURCE_DIR}/thirdparty/misc/fastlz.c
        ${PROJECT_SOURCE_DIR}/thirdparty/misc/smaz.c
        # C++ sources
        ${PROJECT_SOURCE_DIR}/thirdparty/misc/hq2x.cpp
        ${PROJECT_SOURCE_DIR}/thirdparty/misc/pcg.cpp
        ${PROJECT_SOURCE_DIR}/thirdparty/misc/triangulator.cpp
        # Used in scene
        ${PROJECT_SOURCE_DIR}/thirdparty/misc/mikktspace.c

        ${PROJECT_SOURCE_DIR}/thirdparty/misc/clipper.cpp
    )
target_include_directories(thirdparty_misc PUBLIC misc/)
target_link_libraries(thirdparty_misc PUBLIC zlib)
target_link_libraries(thirdparty_misc PRIVATE EASTL_Import)
set_3rdparty_properties(thirdparty_misc)

if(OPTION_MINIZIP)
    set(minizip_headers
        ${PROJECT_SOURCE_DIR}/thirdparty/minizip/unzip.h
        ${PROJECT_SOURCE_DIR}/thirdparty/minizip/zip.h

    )
    add_library(minizip STATIC
        # Minizip library, can be unbundled in theory
        # However, our version has some custom modifications, so it won't compile with the system one
            ${PROJECT_SOURCE_DIR}/thirdparty/minizip/ioapi.c
            ${PROJECT_SOURCE_DIR}/thirdparty/minizip/unzip.c
            ${PROJECT_SOURCE_DIR}/thirdparty/minizip/zip.c
            ${minizip_headers}
        )
    target_include_directories(minizip PUBLIC thirdparty/misc/)
    # export minizip symbols.
    target_link_libraries(minizip PUBLIC zlib)
    target_link_libraries(minizip PRIVATE EASTL_Import)
    set_property(TARGET minizip PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_target_properties(minizip PROPERTIES FOLDER 3rd_party)

    add_library(minizip_interface INTERFACE)
    target_compile_definitions(minizip_interface INTERFACE ZLIB_DLL)
    target_include_directories(minizip_interface INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
	target_link_libraries(minizip_interface INTERFACE zlib_interface)
    install(FILES ${minizip_headers} DESTINATION include/minizip)
    install(TARGETS minizip_interface EXPORT SegsEngine
        INCLUDES DESTINATION include/minizip
    )
# minizip functions need to be accessible from plugins ? set_common_target_properties(minizip)
endif()

if(OPTION_BUILTIN_SQUISH)
    # Thirdparty source files
    set(thirdparty_sources
        ${PROJECT_SOURCE_DIR}/thirdparty/squish/alpha.cpp
        ${PROJECT_SOURCE_DIR}/thirdparty/squish/clusterfit.cpp
        ${PROJECT_SOURCE_DIR}/thirdparty/squish/colourblock.cpp
        ${PROJECT_SOURCE_DIR}/thirdparty/squish/colourfit.cpp
        ${PROJECT_SOURCE_DIR}/thirdparty/squish/colourset.cpp
        ${PROJECT_SOURCE_DIR}/thirdparty/squish/maths.cpp
        ${PROJECT_SOURCE_DIR}/thirdparty/squish/rangefit.cpp
        ${PROJECT_SOURCE_DIR}/thirdparty/squish/singlecolourfit.cpp
        ${PROJECT_SOURCE_DIR}/thirdparty/squish/squish.cpp
    )
    add_library(squish STATIC ${thirdparty_sources})
    target_include_directories(squish PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/squish/)
    set_3rdparty_properties(squish)

endif()

add_library(cvtt_lib OBJECT cvtt/ConvectionKernels.cpp)
target_include_directories(cvtt_lib PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/cvtt/)
set_3rdparty_properties(cvtt_lib)

set(thirdparty_sources
#    ${PROJECT_SOURCE_DIR}/thirdparty/enet/godot.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/enet/callbacks.c
    ${PROJECT_SOURCE_DIR}/thirdparty/enet/compress.c
    ${PROJECT_SOURCE_DIR}/thirdparty/enet/host.c
    ${PROJECT_SOURCE_DIR}/thirdparty/enet/list.c
    ${PROJECT_SOURCE_DIR}/thirdparty/enet/packet.c
    ${PROJECT_SOURCE_DIR}/thirdparty/enet/peer.c
    ${PROJECT_SOURCE_DIR}/thirdparty/enet/protocol.c

)
add_library(enet_lib STATIC ${thirdparty_sources})
target_include_directories(enet_lib PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/enet/)
set_3rdparty_properties(enet_lib)

# Thirdparty source files

macro(remove_flag_from_target _target _flag)
    get_target_property(_target_cxx_flags ${_target} COMPILE_OPTIONS)
    if(_target_cxx_flags)
        list(REMOVE_ITEM _target_cxx_flags ${_flag})
        set_target_properties(${_target} PROPERTIES COMPILE_OPTIONS "${_target_cxx_flags}")
    endif()
endmacro()


set(thirdparty_sources
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_auto_possess.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_chartables.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_compile.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_config.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_context.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_dfa_match.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_error.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_find_bracket.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_jit_compile.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_maketables.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_match.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_match_data.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_newline.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_ord2utf.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_pattern_info.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_serialize.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_string_utils.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_study.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_substitute.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_substring.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_tables.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_ucd.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_valid_utf.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_xclass.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_ucptables.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_convert.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_extuni.c
    ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/pcre2_script_run.c

)
add_library(regex STATIC ${thirdparty_sources})
target_include_directories(regex PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/pcre2/src/)
target_compile_definitions(regex PUBLIC PCRE2_CODE_UNIT_WIDTH=8 PCRE2_STATIC HAVE_CONFIG_H SUPPORT_JIT)
set_3rdparty_properties(regex)

if(OPTION_BUILTIN_MBEDTLS)
    set(thirdparty_sources
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/aes.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/aesni.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/arc4.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/aria.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/asn1parse.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/asn1write.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/base64.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/bignum.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/blowfish.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/camellia.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/ccm.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/certs.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/chacha20.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/chachapoly.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/cipher.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/cipher_wrap.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/cmac.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/ctr_drbg.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/constant_time.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/debug.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/des.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/dhm.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/ecdh.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/ecdsa.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/ecjpake.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/ecp.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/ecp_curves.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/entropy.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/entropy_poll.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/error.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/gcm.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/havege.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/hkdf.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/hmac_drbg.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/md2.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/md4.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/md5.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/md.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/memory_buffer_alloc.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/mps_reader.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/mps_trace.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/net_sockets.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/nist_kw.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/oid.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/padlock.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/pem.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/pk.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/pkcs11.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/pkcs12.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/pkcs5.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/pkparse.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/pk_wrap.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/pkwrite.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/platform.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/platform_util.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/poly1305.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/ripemd160.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/rsa.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/rsa_internal.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/sha1.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/sha256.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/sha512.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/ssl_cache.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/ssl_ciphersuites.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/ssl_cli.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/ssl_cookie.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/ssl_msg.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/ssl_srv.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/ssl_ticket.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/ssl_tls.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/ssl_tls13_keys.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/threading.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/timing.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/version.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/version_features.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/x509.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/x509_create.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/x509_crl.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/x509_crt.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/x509_csr.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/x509write_crt.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/x509write_csr.c
        ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/library/xtea.c
    )
    add_library(mbedtls STATIC ${thirdparty_sources})
    target_include_directories(mbedtls PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/mbedtls/include)
    set_3rdparty_properties(mbedtls)

endif()

add_subdirectory(assimp)

set(thirdparty_sources
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/addr_is_reserved.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/miniupnpc.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/upnpcommands.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/miniwget.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/upnpdev.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/igd_desc_parse.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/minissdpc.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/minisoap.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/minixml.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/connecthostport.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/receivedata.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/portlistingparse.c
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/upnpreplyparse.c
)
configure_file(${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/miniupnpcstrings.h.cmake
    ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/miniupnpc/miniupnpcstrings.h
)
set(MINIUPNPC_VERSION "2.2.2")
set(SYSTEM_NAME "Godot Engine")
add_library(miniupnpc STATIC ${thirdparty_sources})
target_include_directories(miniupnpc PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/miniupnpc/)
target_compile_definitions(miniupnpc PUBLIC MINIUPNP_STATICLIB)
target_compile_definitions(miniupnpc PRIVATE MINIUPNPC_SET_SOCKET_TIMEOUT)
set_3rdparty_properties(miniupnpc)


set(thirdparty_sources
    ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/src/vhacdManifoldMesh.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/src/FloatMath.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/src/vhacdMesh.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/src/vhacdICHull.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/src/vhacdVolume.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/src/VHACD-ASYNC.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/src/btAlignedAllocator.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/src/vhacdRaycastMesh.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/src/VHACD.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/src/btConvexHullComputer.cpp
)
add_library(vhacd OBJECT ${thirdparty_sources})
target_include_directories(vhacd PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/inc)
set_3rdparty_properties(vhacd)

set(thirdparty_sources

    ${PROJECT_SOURCE_DIR}/thirdparty/wslay/wslay_net.c
    ${PROJECT_SOURCE_DIR}/thirdparty/wslay/wslay_event.c
    ${PROJECT_SOURCE_DIR}/thirdparty/wslay/wslay_queue.c
    ${PROJECT_SOURCE_DIR}/thirdparty/wslay/wslay_frame.c
)
add_library(wslay OBJECT ${thirdparty_sources})
target_include_directories(wslay PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/wslay/includes)
set_3rdparty_properties(wslay)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows") # Winsock
    target_compile_definitions(wslay PRIVATE HAVE_WINSOCK2_H)
else() # Unix socket
    target_compile_definitions(wslay PRIVATE HAVE_NETINET_IN_H)
endif()


if(OPTION_BUILTIN_LIBOGG)
    set(thirdparty_sources
        ${PROJECT_SOURCE_DIR}/thirdparty/libogg/bitwise.c
        ${PROJECT_SOURCE_DIR}/thirdparty/libogg/framing.c
    )
    add_library(ogg STATIC ${thirdparty_sources})
    target_include_directories(ogg PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/libogg/)
    set_property(TARGET ogg PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_target_properties(ogg PROPERTIES FOLDER 3rd_party)
endif()

set(thirdparty_sources
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/png.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngerror.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngget.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngmem.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngpread.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngread.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngrio.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngrtran.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngrutil.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngset.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngtrans.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngwio.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngwrite.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngwtran.c
    ${PROJECT_SOURCE_DIR}/thirdparty/libpng/pngwutil.c
)

add_library(internal_png STATIC ${thirdparty_sources})
target_include_directories(internal_png PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/libpng/)
target_link_libraries(internal_png PUBLIC zlib)
set_3rdparty_properties(internal_png)

set(brotli_files
        ${PROJECT_SOURCE_DIR}/thirdparty/brotli/common/constants.c
        ${PROJECT_SOURCE_DIR}/thirdparty/brotli/common/context.c
        ${PROJECT_SOURCE_DIR}/thirdparty/brotli/common/dictionary.c
        ${PROJECT_SOURCE_DIR}/thirdparty/brotli/common/platform.c
        ${PROJECT_SOURCE_DIR}/thirdparty/brotli/common/shared_dictionary.c
        ${PROJECT_SOURCE_DIR}/thirdparty/brotli/common/transform.c
        ${PROJECT_SOURCE_DIR}/thirdparty/brotli/dec/bit_reader.c
        ${PROJECT_SOURCE_DIR}/thirdparty/brotli/dec/decode.c
        ${PROJECT_SOURCE_DIR}/thirdparty/brotli/dec/huffman.c
        ${PROJECT_SOURCE_DIR}/thirdparty/brotli/dec/state.c
)
add_library(brotli STATIC ${brotli_files})
target_include_directories(brotli PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/brotli/include)
set_3rdparty_properties(brotli)

set(source_files
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/autofit/autofit.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftbase.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftbbox.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftbdf.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftbitmap.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftcid.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftdebug.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftfstype.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftgasp.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftglyph.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftgxval.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftinit.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftmm.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftotval.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftpatent.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftpfr.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftstroke.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftsynth.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftsystem.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/fttype1.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftwinfnt.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/bdf/bdf.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/bzip2/ftbzip2.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/cache/ftcache.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/cff/cff.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/cid/type1cid.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/gxvalid/gxvalid.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/gzip/ftgzip.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/lzw/ftlzw.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/otvalid/otvalid.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/pcf/pcf.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/pfr/pfr.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/psaux/psaux.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/pshinter/pshinter.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/psnames/psnames.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/raster/raster.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/sdf/sdf.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/svg/svg.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/smooth/smooth.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/truetype/truetype.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/type1/type1.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/type42/type42.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/winfonts/winfnt.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/sfnt/sfnt.c
)
add_library(freetype STATIC ${source_files})
target_include_directories(freetype PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/freetype/include)
target_compile_definitions(freetype PRIVATE FT2_BUILD_LIBRARY)
target_compile_definitions(freetype PUBLIC FREETYPE_ENABLED FT_CONFIG_OPTION_USE_PNG FT_CONFIG_OPTION_USE_BROTLI)
target_link_libraries(freetype PUBLIC internal_png brotli)
set_3rdparty_properties(freetype)

set(recast_source_files
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/Recast.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/RecastAlloc.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/RecastArea.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/RecastAssert.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/RecastContour.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/RecastFilter.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/RecastLayers.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/RecastMesh.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/RecastMeshDetail.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/RecastRasterization.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/RecastRegion.cpp

)
add_library(recast STATIC ${recast_source_files})
target_include_directories(recast PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Include)
set_property(TARGET recast PROPERTY POSITION_INDEPENDENT_CODE ON)
set_3rdparty_properties(recast)

set(source_files
    ${PROJECT_SOURCE_DIR}/thirdparty/glad/glad.c
    ${PROJECT_SOURCE_DIR}/thirdparty/glad/glad/glad.h
)
add_library(glad STATIC ${source_files})
target_include_directories(glad PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/glad/)
set_3rdparty_properties(glad)

if(USE_TRACY_PROFILER)
    add_subdirectory(glfw)
    add_subdirectory(tracy)
endif()

set(thirdparty_sources
    ${PROJECT_SOURCE_DIR}/thirdparty/rvo2/Agent.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/rvo2/Agent.h
    ${PROJECT_SOURCE_DIR}/thirdparty/rvo2/Definitions.h
    ${PROJECT_SOURCE_DIR}/thirdparty/rvo2/KdTree.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/rvo2/KdTree.h
    ${PROJECT_SOURCE_DIR}/thirdparty/rvo2/Vector3.h
)
add_library(rvo2 OBJECT ${thirdparty_sources})
target_include_directories(rvo2 PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/rvo2)
target_include_directories(rvo2 INTERFACE ${PROJECT_SOURCE_DIR}/thirdparty)  # allow using rvo2 headers externally by rvo2/Header.h
set_3rdparty_properties(rvo2)


set(concurrent_queue_SRC
    ${PROJECT_SOURCE_DIR}/thirdparty/concurrentqueue/blockingconcurrentqueue.h
    ${PROJECT_SOURCE_DIR}/thirdparty/concurrentqueue/concurrentqueue.h
    ${PROJECT_SOURCE_DIR}/thirdparty/concurrentqueue/lightweightsemaphore.h
)
add_library(concurrent_queue INTERFACE)

target_include_directories(concurrent_queue INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
install(FILES ${concurrent_queue_SRC} DESTINATION include/concurrentqueue)
install(TARGETS concurrent_queue EXPORT SegsEngine
    INCLUDES DESTINATION include/concurrentqueue
)

set(stb_pack_SRC
    ${PROJECT_SOURCE_DIR}/thirdparty/stb_rect_pack/stb_rect_pack.h
)
add_library(stb_rect_pack INTERFACE)

target_include_directories(stb_rect_pack INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

set(minimp3_SRC
    ${PROJECT_SOURCE_DIR}/thirdparty/minimp3/minimp3.h
    ${PROJECT_SOURCE_DIR}/thirdparty/minimp3/minimp3_ex.h
)
add_library(minimp3 INTERFACE)

target_include_directories(minimp3 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

set_3rdparty_properties(bullet)
set_3rdparty_properties(assimp_lib)
set_3rdparty_properties(zlib)
set_target_properties(EASTL PROPERTIES FOLDER 3rd_party)
