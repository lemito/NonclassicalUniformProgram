cmake_minimum_required(VERSION 3.16)
project(fuzzy VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(FUZZY_SOURCES
    src/rng.cpp
    src/log.cpp
    src/fuzzy_number.cpp
    src/generator.cpp
    src/selection.cpp
    src/cluster.cpp
)

add_library(fuzzy_static STATIC ${FUZZY_SOURCES})
set_target_properties(fuzzy_static PROPERTIES OUTPUT_NAME fuzzy)
target_include_directories(fuzzy_static PUBLIC include)

add_library(fuzzy_shared SHARED ${FUZZY_SOURCES})
set_target_properties(fuzzy_shared PROPERTIES OUTPUT_NAME fuzzy VERSION ${PROJECT_VERSION} SOVERSION 1
    CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(fuzzy_shared PRIVATE FUZZY_BUILD_SHARED)
target_include_directories(fuzzy_shared PUBLIC include)

foreach(t fuzzy_static fuzzy_shared)
    if(MSVC)
        target_compile_options(${t} PRIVATE /W4)
    else()
        target_compile_options(${t} PRIVATE -Wall -Wextra)
    endif()
endforeach()

enable_testing()
add_executable(fuzzy_tests tests/test_main.cpp)
target_link_libraries(fuzzy_tests PRIVATE fuzzy_static)
add_test(NAME fuzzy_tests COMMAND fuzzy_tests)

install(TARGETS fuzzy_static fuzzy_shared ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
install(FILES include/fuzzy.h DESTINATION include)
