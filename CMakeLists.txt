cmake_minimum_required(VERSION 3.9)
project(readerwriterqueue VERSION 1.0.0)

include(GNUInstallDirs)

add_library(${PROJECT_NAME} INTERFACE)

install(FILES atomicops.h readerwriterqueue.h LICENSE.md
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})
