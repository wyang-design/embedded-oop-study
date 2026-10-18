 cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)




project(smartheat 
        VERSION 0.1
        DESCRIPTION "Smart Heat"
        LANGUAGES C)


add_executable(smartheat main.c)

add_subdirectory(hal)
add_subdirectory(domain)
add_subdirectory(adapters)

add_subdirectory(ports)

target_link_libraries(smartheat PRIVATE hal domain adapters ports)

target_include_directories(smartheat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})