#
# A CMake Module for finding Mono.
#
# The following variables are set:
#   CSHARP_MONO_FOUND
#   CSHARP_MONO_COMPILER eg. "CSHARP_MONO_COMPILER_2.10.2"
#   CSHARP_MONO_INTERPRETER eg. "CSHARP_MONO_INTERPRETOR_2.10.2"
#   CSHARP_MONO_VERSION eg. "2.10.2"
#
# Additional references can be found here:
#   http://www.mono-project.com/Main_Page
#   http://www.mono-project.com/CSharp_Compiler
#   http://mono-project.com/FAQ:_Technical (How can I tell where the Mono runtime is installed)
#
# This file is based on the work of GDCM:
#   http://gdcm.svn.sf.net/viewvc/gdcm/trunk/CMake/FindMono.cmake
# Copyright (c) 2006-2010 Mathieu Malaterre <mathieu.malaterre@gmail.com>
#

set( csharp_mono_valid 1 )
#if( DEFINED CSHARP_MONO_FOUND )
  # The Mono compiler has already been found
  # It may have been reset by the user, verify it is correct
#  if( NOT DEFINED CSHARP_MONO_COMPILER )
#    set( csharp_mono_version_user ${CSHARP_MONO_VERSION} )
#    set( csharp_mono_valid 0 )
#    set( CSHARP_MONO_FOUND 0 )
#    set( CSHARP_MONO_VERSION "CSHARP_MONO_VERSION-NOTVALID" CACHE STRING "C# Mono compiler version, choices: ${CSHARP_MONO_VERSIONS}" FORCE )
#    message( FATAL_ERROR "The C# Mono version '${csharp_mono_version_user}' is not valid. Please enter one of the following: ${CSHARP_MONO_VERSIONS}" )
#  endif( NOT DEFINED CSHARP_MONO_COMPILER_${CSHARP_MONO_VERSION} )
#endif( DEFINED CSHARP_MONO_FOUND )

unset( CSHARP_MONO_VERSIONS CACHE ) # Clear versions
if( WIN32 )
  # Search for Mono on Win32 systems
  # See http://mono-project.com/OldReleases and http://www.go-mono.com/mono-downloads/download.html
  set( csharp_mono_bin_dirs )
  set( csharp_mono_search_hint
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Mono;SdkInstallRoot]"
  )
  get_filename_component( csharp_mono_bin_dir "${csharp_mono_search_hint}" ABSOLUTE )
  message(${csharp_mono_bin_dir})
  if ( NOT EXISTS "${csharp_mono_bin_dir}" )
    message("Mono directory given in registry does not exist")
    return()
  endif()

  string( REPLACE "\\" "/" csharp_mono_bin_dir ${csharp_mono_bin_dir} )
  if (EXISTS "${csharp_mono_bin_dir}/bin/dmcs.bat")
    set( csharp_mono_executable "${csharp_mono_bin_dir}/bin/dmcs.bat")
  elseif (EXISTS "${csharp_mono_bin_dir}/bin/gmcs.bat")
    set( csharp_mono_executable "${csharp_mono_bin_dir}/bin/gmcs.bat")
  elseif (EXISTS "${csharp_mono_bin_dir}/bin/mcs.bat")
    set( csharp_mono_executable "${csharp_mono_bin_dir}/bin/mcs.bat")
  endif()

  if( csharp_mono_valid )
    # Extract version number (eg. 2.10.2)
     # Add variable holding executable

    # Set interpreter
    if (EXISTS "${csharp_mono_bin_dir}/bin/mono.exe")
      execute_process(
        COMMAND "${csharp_mono_bin_dir}/bin/mono.exe" -V
        OUTPUT_VARIABLE csharp_mono_version_string
      )
      string( REGEX MATCH "([0-9]*)([.])([0-9]*)([.]*)([0-9]*)" csharp_mono_version_temp "${csharp_mono_version_string}" )
      message("Candidate mono version ${csharp_mono_version_temp}")

      set( CSHARP_MONO_INTERPRETER "${csharp_mono_bin_dir}/bin/mono.exe" CACHE STRING "C# Mono interpreter ${csharp_mono_version_temp}" FORCE )
      mark_as_advanced( CSHARP_MONO_INTERPRETER )
      set( CSHARP_MONO_COMPILER ${csharp_mono_executable} CACHE STRING "C# Mono compiler ${csharp_mono_version_temp}" FORCE )
      mark_as_advanced( CSHARP_MONO_COMPILER )
    endif ()
  endif()

  # Create a list of supported compiler versions
  if( NOT DEFINED CSHARP_MONO_VERSIONS )
    set( CSHARP_MONO_VERSIONS "${csharp_mono_version_temp}" CACHE STRING "Available C# Mono compiler versions" FORCE )
  else( NOT DEFINED CSHARP_MONO_VERSIONS )
    set( CSHARP_MONO_VERSIONS "${CSHARP_MONO_VERSIONS}, ${csharp_mono_version_temp}"  CACHE STRING "Available C# Mono versions" FORCE )
  endif( NOT DEFINED CSHARP_MONO_VERSIONS )
  mark_as_advanced( CSHARP_MONO_VERSIONS )
 # We found at least one Mono compiler version
  set( CSHARP_MONO_FOUND 1 CACHE INTERNAL "Boolean indicating if C# Mono was found" )
  add_library(Mono SHARED IMPORTED GLOBAL)
  set_target_properties(Mono PROPERTIES IMPORTED_IMPLIB ${csharp_mono_bin_dir}/lib/mono-2.0-sgen.lib)
  target_link_directories(Mono INTERFACE ${csharp_mono_bin_dir}/lib)
  target_include_directories(Mono INTERFACE ${csharp_mono_bin_dir}/include/mono-2.0)
  if(MINGW)
    target_compile_options(Mono INTERFACE -mms-bitfields)
  else()
    set(RUN_PATH "${csharp_mono_bin_dir}/bin;${CMAKE_MSVCIDE_RUN_PATH}")
    list(REMOVE_DUPLICATES RUN_PATH)
    set(CMAKE_MSVCIDE_RUN_PATH "${RUN_PATH}" CACHE STATIC "MSVC IDE Run path" FORCE)
  endif()
elseif( UNIX )
  # Remove temp variable from cache
  unset( csharp_mono_compiler CACHE )

  include(FindPkgConfig)  # we don't need the pkg-config path on OS X, but we need other macros in this file
  pkg_check_modules(Mono REQUIRED IMPORTED_TARGET GLOBAL mono-2)
  add_library(Mono ALIAS PkgConfig::Mono)
endif()

if( CSHARP_MONO_FOUND )
  # Report the found versions
  message( STATUS "Found the following C# Mono versions: ${CSHARP_MONO_VERSIONS}" )
endif()
