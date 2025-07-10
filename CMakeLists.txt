cmake_minimum_required(VERSION 3.10)
project(SystemMonitor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(SOURCE_FILES
    src/main.cpp
    src/system_data.cpp
    src/system_data.h
)

option(USE_GTK "Build with GTK+ GUI" ON)

set(LINK_LIBRARIES "")
set(COMPILE_DEFINITIONS "")
set(INCLUDE_DIRECTORIES "")

if(USE_GTK)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    if(GTK3_FOUND)
        message(STATUS "Found GTK3: ${GTK3_LIBRARIES}")
        string(STRIP "${GTK3_LIBRARIES}" GTK3_LIBRARIES_STRIPPED)

        list(APPEND COMPILE_DEFINITIONS USE_GTK)

        list(APPEND INCLUDE_DIRECTORIES ${GTK3_INCLUDE_DIRS})

        list(APPEND SOURCE_FILES src/gui_manager.cpp src/gui_manager.h)
        list(APPEND LINK_LIBRARIES ${GTK3_LIBRARIES_STRIPPED})
    else()
        message(FATAL_ERROR "GTK3 not found. Set USE_GTK to OFF or install GTK3 development files.")
    endif()
else()
    find_package(Curses REQUIRED)
    if(CURSES_FOUND)
        message(STATUS "Found Curses: ${CURSES_LIBRARIES}")
        string(STRIP "${CURSES_LIBRARIES}" CURSES_LIBRARIES_STRIPPED)

        list(APPEND INCLUDE_DIRECTORIES ${CURSES_INCLUDE_DIRS})

        list(APPEND SOURCE_FILES src/terminal_ui.cpp src/terminal_ui.h)
        list(APPEND LINK_LIBRARIES ${CURSES_LIBRARIES_STRIPPED})
    else()
        message(FATAL_ERROR "Curses (ncurses) not found. Set USE_GTK to ON or install ncurses development files.")
    endif()
endif()

if(NOT SOURCE_FILES)
    message(FATAL_ERROR "No source files specified. Check USE_GTK option and file paths.")
endif()

add_executable(system_monitor ${SOURCE_FILES})

if(COMPILE_DEFINITIONS)
    target_compile_definitions(system_monitor PRIVATE ${COMPILE_DEFINITIONS})
endif()

target_include_directories(system_monitor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${INCLUDE_DIRECTORIES}
)

target_link_libraries(system_monitor PRIVATE ${LINK_LIBRARIES})