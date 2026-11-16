cmake_minimum_required(VERSION 3.21)
project(fuzzy_desktop LANGUAGES C CXX)

set(CEF_VERSION "156.0.14+g4fa4ab5+chromium-156.0.8078.48" CACHE STRING "CEF version")
set(CEF_SHA1 "d091c82a8655113a34616c08cb340bf92310298d" CACHE STRING "CEF archive SHA1")
set(CEF_PLATFORM "linux64")
set(CEF_DIST "cef_binary_${CEF_VERSION}_${CEF_PLATFORM}_minimal")
set(CEF_DOWNLOAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party" CACHE PATH "CEF download directory")
set(CEF_ROOT "${CEF_DOWNLOAD_DIR}/${CEF_DIST}")
set(FUZZY_SERVER_BIN "${CMAKE_CURRENT_SOURCE_DIR}/../bin/fuzzy-server" CACHE FILEPATH "fuzzy-server binary")

if(NOT EXISTS "${CEF_ROOT}/cmake/FindCEF.cmake")
    string(REPLACE "+" "%2B" CEF_DIST_URL "${CEF_DIST}")
    set(CEF_ARCHIVE "${CEF_DOWNLOAD_DIR}/${CEF_DIST}.tar.bz2")
    message(STATUS "Downloading ${CEF_DIST}.tar.bz2")
    file(DOWNLOAD "https://cef-builds.spotifycdn.com/${CEF_DIST_URL}.tar.bz2" "${CEF_ARCHIVE}"
        EXPECTED_HASH SHA1=${CEF_SHA1} SHOW_PROGRESS)
    file(ARCHIVE_EXTRACT INPUT "${CEF_ARCHIVE}" DESTINATION "${CEF_DOWNLOAD_DIR}")
    file(REMOVE "${CEF_ARCHIVE}")
endif()

list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)

add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

set(DESKTOP_SOURCES
    src/main.cpp
    src/app.cpp
    src/client.cpp
    src/server_process.cpp
)

set(CEF_TARGET fuzzy-desktop)
set(CEF_TARGET_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}")

ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")
SET_CEF_TARGET_OUT_DIR()

add_executable(${CEF_TARGET} ${DESKTOP_SOURCES})
SET_EXECUTABLE_TARGET_PROPERTIES(${CEF_TARGET})
add_dependencies(${CEF_TARGET} libcef_dll_wrapper)
target_link_libraries(${CEF_TARGET} libcef_lib libcef_dll_wrapper ${CEF_STANDARD_LIBS})
set_target_properties(${CEF_TARGET} PROPERTIES INSTALL_RPATH "$ORIGIN" BUILD_WITH_INSTALL_RPATH TRUE)

COPY_FILES("${CEF_TARGET}" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${CEF_TARGET_OUT_DIR}")
COPY_FILES("${CEF_TARGET}" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${CEF_TARGET_OUT_DIR}")

if(EXISTS "${FUZZY_SERVER_BIN}")
    add_custom_command(TARGET ${CEF_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FUZZY_SERVER_BIN}" "${CEF_TARGET_OUT_DIR}/fuzzy-server")
else()
    message(WARNING "fuzzy-server not found at ${FUZZY_SERVER_BIN}; build it with 'make server' or pass --url")
endif()
