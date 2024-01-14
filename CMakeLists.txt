cmake_minimum_required(VERSION 3.9)
set(CMAKE_CXX_STANDARD 11)  # don't need to specifiy
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

project(readerwriterqueue)

add_library(readerwriterqueue INTERFACE)

# This needs to be updated everytime the library tag version updates. The version is not 1.0.0, it's 1.0.6 according to tags.
set_target_properties(readerwriterqueue PROPERTIES
        SOVERSION 1
        VERSION 1.0.6)

#see this talk for why these changes need to be made https://www.youtube.com/watch?v=m0DwB4OvDXk

target_include_directories(readerwriterqueue INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}>
)

set_target_properties(readerwriterqueue PROPERTIES EXPORT_NAME readerwriterqueue)

# This is here to ensure parity with the install interface, but won't hide the previous target name.
add_library(moodycamel::readerwriterqueue ALIAS readerwriterqueue)




install(TARGETS readerwriterqueue
        EXPORT readerwriterqueue_Targets
        COMPONENT readerwriterqueue_Development
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT creaderwriterqueue_RunTime
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT readerwriterqueue_RunTIme
        NAMELINK_COMPONENT readerwriterqueue_Development
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT readerwriterqueue_Development
)

set(readerwriterqueue_INSTALL_CMAKEDIR
        ${CMAKE_INSTALL_DATADIR}/readerwriterqueue
        CACHE STRING "Path to readerwriterqueue cmake files"
)

install(EXPORT readerwriterqueue_Targets
        DESTINATION ${readerwriterqueue_INSTALL_CMAKEDIR}
        NAMESPACE moodycamel::
        FILE readerwriterqueueTargets.cmake
        COMPONENT readerwriterqueue_Development
)

include(CMakePackageConfigHelpers)
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/readerwriterqueueConfig.cmake"
        INSTALL_DESTINATION ${readerwriterqueue_INSTALL_CMAKEDIR}
)


get_target_property(readerwriterqueue_VERSION readerwriterqueue VERSION)

write_basic_package_version_file(readerwriterqueueConfigVersion.cmake VERSION ${readerwriterqueue_VERSION} COMPATIBILITY SameMajorVersion)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/readerwriterqueueConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/readerwriterqueueConfigVersion.cmake
        DESTINATION ${readerwriterqueue_INSTALL_CMAKEDIR}
)


install(FILES
        atomicops.h
        readerwriterqueue.h
        readerwritercircularbuffer.h
        LICENSE.md
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})


option(MOODYCAMEL_READERWRITERQUEUE_ENABLE_TESTS "Enables readerwriterqueue tests" OFF)

if(${MOODYCAMEL_READERWRITERQUEUE_ENABLE_TESTS})

    find_package(Threads REQUIRED)
#    add_library(simple)
    add_executable(unittests)
    target_sources(unittests PRIVATE
            tests/unittests/minitest.h
            tests/unittests/unittests.cpp
            tests/common/simplethread.h
            tests/common/simplethread.cpp
    )
    target_include_directories(unittests PRIVATE
            tests/unittests/)
#    static.
    set_property(TARGET unittests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    # handles pthread and such.
    target_link_libraries(unittests PRIVATE Threads::Threads)
#    target_link_libraries(unittests PRIVATE readerwriterqueue)
    target_compile_options(unittests
            PRIVATE
            $<$<CXX_COMPILER_ID:Clang>: -Wsign-conversion -Wpedantic -Wall -DNDEBUG -O3 -g>
            $<$<CXX_COMPILER_ID:GNU>:  -Wsign-conversion -Wpedantic -Wall -DNDEBUG -O3 -g>
#            /W4 is similar to -Wall, next are similar to -sign-conversion, /permissive- is similar to -Wpedantic.
            $<$<CXX_COMPILER_ID:MSVC>: /W4 /w14287 /w14826 /permissive- /02 /DEBUG>
    )
    target_link_options(unittests
            PRIVATE
            $<$<CXX_COMPILER_ID:Clang>: -lrt -Wl,--no-as-needed>
            $<$<CXX_COMPILER_ID:GNU>: -lrt -Wl,--no-as-needed>
    )

    #    add_library(simple)
    add_executable(stabtest)
    target_sources(stabtest PRIVATE
            tests/stabtest/stabtest.cpp
            tests/common/simplethread.h
            tests/common/simplethread.cpp
    )
    target_include_directories(stabtest PRIVATE
            tests/stabtest/)
    #    static.
    set_property(TARGET stabtest PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    # handles pthread and such.
    target_link_libraries(stabtest PRIVATE Threads::Threads)
    #    target_link_libraries(unittests PRIVATE readerwriterqueue)
    target_compile_options(stabtest
            PRIVATE
            $<$<CXX_COMPILER_ID:Clang>: -Wsign-conversion -Wpedantic -Wall -DNDEBUG -O3 -g>
            $<$<CXX_COMPILER_ID:GNU>:  -Wsign-conversion -Wpedantic -Wall -DNDEBUG -O3 -g>
            #            /W4 is similar to -Wall, next are similar to -sign-conversion, /permissive- is similar to -Wpedantic.
            $<$<CXX_COMPILER_ID:MSVC>: /W4 /w14287 /w14826 /permissive- /02 /DEBUG>
    )
    target_link_options(stabtest
            PRIVATE
            $<$<CXX_COMPILER_ID:Clang>: -lrt -Wl,--no-as-needed>
            $<$<CXX_COMPILER_ID:GNU>: -lrt -Wl,--no-as-needed>
    )


endif()