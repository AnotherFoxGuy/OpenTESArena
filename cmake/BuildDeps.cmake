cmake_minimum_required(VERSION 3.2)

include(ExternalProject)

# Set the EP_BASE directory property to setup the build directory structure (see the
# ExternalProject documentation for more information)
set_directory_properties(PROPERTIES EP_BASE ${CMAKE_BINARY_DIR}/dependencies)

set(DEPS_OUTPUT_DIR ${CMAKE_BINARY_DIR}/dependencies/Output)
set(DEPS_CMAKE_BUILD_TYPE $<IF:$<CONFIG:Release>,Release,Debug>)
set(DEPS_CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj /w")
set(DEPS_INCLUDE_DIR ${DEPS_OUTPUT_DIR}/include)
set(DEPS_LIB_DIR ${DEPS_OUTPUT_DIR}/lib)
set(DEPS_BIN_DIR ${DEPS_OUTPUT_DIR}/bin)


file(MAKE_DIRECTORY ${DEPS_INCLUDE_DIR})
file(MAKE_DIRECTORY ${DEPS_LIB_DIR})
file(MAKE_DIRECTORY ${DEPS_BIN_DIR})

# ------------------------------------------------------------------------------------------------#
#  SDL2
# ------------------------------------------------------------------------------------------------#

ExternalProject_Add(
		SDL2
		URL https://www.libsdl.org/release/SDL2-2.0.7.tar.gz
		URL_MD5 cdb071009d250e1782371049f0d5ca42
		UPDATE_COMMAND ""
		CMAKE_ARGS
		-DCMAKE_INSTALL_PREFIX=${DEPS_OUTPUT_DIR}
		-DCMAKE_BUILD_TYPE=${DEPS_CMAKE_BUILD_TYPE}
		-DCMAKE_CXX_FLAGS=${DEPS_CMAKE_CXX_FLAGS}
)

SET(SDL2_FOUND TRUE)
SET(SDL2_LIBRARY ${DEPS_LIB_DIR}/SDL2.lib)
SET(SDL2_INCLUDE_DIR  ${DEPS_INCLUDE_DIR}/SDL2)

# ------------------------------------------------------------------------------------------------#
#  OpenAL
# ------------------------------------------------------------------------------------------------#

ExternalProject_Add(
		OPENAL
		GIT_REPOSITORY https://github.com/kcat/openal-soft
		GIT_TAG openal-soft-1.17.1
		UPDATE_COMMAND ""
		CMAKE_ARGS
		-DCMAKE_INSTALL_PREFIX=${DEPS_OUTPUT_DIR}
		-DCMAKE_BUILD_TYPE=${DEPS_CMAKE_BUILD_TYPE}
		-DCMAKE_CXX_FLAGS=${DEPS_CMAKE_CXX_FLAGS}
)
SET(OPENAL_FOUND TRUE)
SET(OPENAL_LIBRARY ${DEPS_LIB_DIR}/OpenAL32.lib)
SET(OPENAL_INCLUDE_DIR  ${DEPS_INCLUDE_DIR}/AL)

# ------------------------------------------------------------------------------------------------#
#  WildMIDI
# ------------------------------------------------------------------------------------------------#

set( WILDMIDI_BUILD_DIR ${CMAKE_BINARY_DIR}/dependencies/Build/WILDMIDI )
set( WILDMIDI_SOURCE_DIR ${CMAKE_BINARY_DIR}/dependencies/Source/WILDMIDI )

ExternalProject_Add(
		WILDMIDI
		GIT_REPOSITORY https://github.com/Mindwerks/wildmidi.git
		GIT_TAG wildmidi-0.4.1
		UPDATE_COMMAND ""
		CMAKE_ARGS
		-DCMAKE_INSTALL_PREFIX=${DEPS_OUTPUT_DIR}
		-DCMAKE_BUILD_TYPE=${DEPS_CMAKE_BUILD_TYPE}
		-DCMAKE_CXX_FLAGS=${DEPS_CMAKE_CXX_FLAGS}
		-DWANT_PLAYER=FALSE
		-DWANT_MP_BUILD=TRUE
		INSTALL_COMMAND ${CMAKE_COMMAND} -E copy ${WILDMIDI_BUILD_DIR}/${DEPS_CMAKE_BUILD_TYPE}/wildmidi_dynamic.dll ${DEPS_BIN_DIR}/wildmidi_dynamic.dll
		COMMAND ${CMAKE_COMMAND} -E copy ${WILDMIDI_BUILD_DIR}/${DEPS_CMAKE_BUILD_TYPE}/wildmidi_dynamic.lib ${DEPS_LIB_DIR}/wildmidi_dynamic.lib
		COMMAND	${CMAKE_COMMAND} -E copy_directory ${WILDMIDI_SOURCE_DIR}/include/ ${DEPS_INCLUDE_DIR}/WILDMIDI/
)

SET(WILDMIDI_FOUND TRUE)
SET(WILDMIDI_LIBRARIES ${DEPS_LIB_DIR}/wildmidi_dynamic.lib)
SET(WILDMIDI_INCLUDE_DIRS  ${DEPS_INCLUDE_DIR}/WILDMIDI)
