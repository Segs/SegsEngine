file(GLOB source_files "*.cpp")
file(GLOB headers_files "*.h")
list(APPEND core_sources ${source_files} ${headers_files})
set(core_sources ${core_sources} PARENT_SCOPE)

if(NOT OPTION_BUILTIN_MBEDTLS)
    # add required mbedtls files by hand to support CryptoCore
endif()
