cmake_minimum_required(VERSION 3.10)
include(CheckSymbolExists)

project(CpS230_Program1_CHIP8)

# CMake module/package to set up SDL2 dependency (thanks, trenki2)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

# Foreground/background color defintions
add_definitions(-DGUI8_FG_R=0xFF -DGUI8_FG_G=0xCC -DGUI8_FG_B=0 -DGUI8_BG_R=0x99 -DGUI8_BG_G=0x66 -DGUI8_BG_B=0)

# Graphical host of the CHIP-8 simulator core
add_executable(gui8 gui.c chip8.c)
target_link_libraries(gui8 ${SDL2_LIBRARIES})
check_symbol_exists("floorf" "math.h" HAS_FLOORF)
if(NOT HAS_FLOORF)
    target_link_libraries(gui8 m)
endif()

# Command-line unit test harness for CHIP-8 simulator core
add_executable(test8 test.c chip8.c)

