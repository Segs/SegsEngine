# Open Asset Import Library (assimp)
# ----------------------------------------------------------------------
# Copyright (c) 2006-2019, assimp team
#
# All rights reserved.
#
# Redistribution and use of this software in source and binary forms,
# with or without modification, are permitted provided that the
# following conditions are met:
#
# * Redistributions of source code must retain the above
#   copyright notice, this list of conditions and the
#   following disclaimer.
#
# * Redistributions in binary form must reproduce the above
#   copyright notice, this list of conditions and the
#   following disclaimer in the documentation and/or other
#   materials provided with the distribution.
#
# * Neither the name of the assimp team, nor the names of its
#   contributors may be used to endorse or promote products
#   derived from this software without specific prior
#   written permission of the assimp team.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#----------------------------------------------------------------------
SET(CMAKE_POLICY_DEFAULT_CMP0074 NEW)


#file(GLOB assimp_source_files4 "${PROJECT_SOURCE_DIR}/thirdparty/assimp/code/FBX/*.cpp")
#file(GLOB assimp_source_files5 "${PROJECT_SOURCE_DIR}/thirdparty/assimp/code/MMD/*.cpp")
#file(GLOB assimp_source_files6 "${PROJECT_SOURCE_DIR}/thirdparty/assimp/code/glTF/*.cpp")
#file(GLOB assimp_source_files7 "${PROJECT_SOURCE_DIR}/thirdparty/assimp/code/glTF2/*.cpp")
#target_sources(assimp_lib PRIVATE
#    ${assimp_source_files4}
#    ${assimp_source_files5}
#    ${assimp_source_files6}
#    ${assimp_source_files7}
#)
CONFIGURE_FILE(
  ${CMAKE_CURRENT_LIST_DIR}/include/assimp/config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/include/assimp/config.h
)
INCLUDE_DIRECTORIES( BEFORE
  ./
  code/
  include
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/include
)
set(ASSIMP_BUILD_BVH_IMPORTER TRUE)
set(ASSIMP_BUILD_DXF_IMPORTER TRUE)
set(ASSIMP_BUILD_FBX_IMPORTER TRUE)
set(ASSIMP_BUILD_OFF_IMPORTER TRUE)
set(ASSIMP_BUILD_MMD_IMPORTER TRUE)
set(ASSIMP_BUILD_LWS_IMPORTER TRUE)
set(ASSIMP_BUILD_LWO_IMPORTER TRUE)
add_subdirectory(code)

