cmake_minimum_required(VERSION 2.8)

project(drock)

include_directories(src)
include_directories(include)

SET(ENV{PKG_CONFIG_PATH}
    ${CMAKE_BINARY_DIR}:$ENV{PKG_CONFIG_PATH}
    )

# Yes, we have to add the directory at least
if(NOT TARGET domaingraph)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../domaingraph domaingraph)
endif(NOT TARGET domaingraph)
if(NOT TARGET hypergraph)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../hypergraph hypergraph)
endif(NOT TARGET hypergraph)

find_package(PkgConfig)

pkg_check_modules(drock_PKGCONFIG REQUIRED
    hypergraph domaingraph yaml-cpp
    )

include_directories(${drock_PKGCONFIG_INCLUDE_DIRS})
link_directories(${drock_PKGCONFIG_LIBRARY_DIRS})
add_definitions(${drock_PKGCONFIG_CFLAGS_OTHER} -ggdb --pedantic -Wall -std=c++11)

set(SOURCES
    src/ComputationDomain.cpp
    src/ImportModel.cpp
    )
set(HEADERS
    include/ComputationDomain.hpp
    )

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(drock STATIC ${SOURCES})
target_link_libraries(drock
    ${drock_PKGCONFIG_STATIC_LIBRARIES}
    )

add_executable(drock-import-model src/ImportModel.cpp)
target_link_libraries(drock-import-model drock)
install(TARGETS drock-import-model
RUNTIME DESTINATION bin)

# pkg-config, to be installed:
configure_file(${PROJECT_NAME}.pc.in ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc DESTINATION lib/pkgconfig)
# pkg-config for use inside our buildtree
configure_file(${PROJECT_NAME}-uninstalled.pc.in ${CMAKE_BINARY_DIR}/${PROJECT_NAME}-uninstalled.pc @ONLY)
