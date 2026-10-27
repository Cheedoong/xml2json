# xml2json: a header only XML <-> JSON converter.
#
#   cmake -S . -B build
#   cmake --build build
#   ctest --test-dir build
#
# Consumers can either add this directory with add_subdirectory() and link
# against xml2json::xml2json, or install it and use find_package(xml2json).

cmake_minimum_required(VERSION 3.16)

project(xml2json
        VERSION 2.0.0
        DESCRIPTION "Header only XML to JSON and JSON to XML converter"
        LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# The library itself: header only, so an INTERFACE target.
# ---------------------------------------------------------------------------
add_library(xml2json INTERFACE)
add_library(xml2json::xml2json ALIAS xml2json)
target_compile_features(xml2json INTERFACE cxx_std_11)
target_include_directories(xml2json INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

if(MSVC)
    set(XML2JSON_WARNINGS /W4)
else()
    set(XML2JSON_WARNINGS -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# Command line tool.
# ---------------------------------------------------------------------------
add_executable(xml2json_cli xml2json.cpp)
set_target_properties(xml2json_cli PROPERTIES OUTPUT_NAME xml2json)
target_link_libraries(xml2json_cli PRIVATE xml2json::xml2json)
target_compile_options(xml2json_cli PRIVATE ${XML2JSON_WARNINGS})

# ---------------------------------------------------------------------------
# Tests.
# ---------------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
    add_executable(xml2json_test test/test.cpp test/test_link.cpp)
    target_link_libraries(xml2json_test PRIVATE xml2json::xml2json)
    target_compile_options(xml2json_test PRIVATE ${XML2JSON_WARNINGS})

    add_executable(xml2json_oom_test test/test_oom.cpp)
    target_link_libraries(xml2json_oom_test PRIVATE xml2json::xml2json)
    target_compile_options(xml2json_oom_test PRIVATE ${XML2JSON_WARNINGS})

    add_test(NAME xml2json.regression COMMAND xml2json_test)
    add_test(NAME xml2json.oom COMMAND xml2json_oom_test)
    # The fixtures are located relative to the current directory.
    set_tests_properties(xml2json.regression PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# ---------------------------------------------------------------------------
# Installation.
# ---------------------------------------------------------------------------
install(TARGETS xml2json_cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(TARGETS xml2json
        EXPORT xml2jsonTargets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(FILES include/xml2json.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(DIRECTORY include/rapidjson include/rapidxml
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT xml2jsonTargets
        FILE xml2jsonTargets.cmake
        NAMESPACE xml2json::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xml2json)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/xml2jsonConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/xml2jsonConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xml2json)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/xml2jsonConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/xml2jsonConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/xml2jsonConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xml2json)
