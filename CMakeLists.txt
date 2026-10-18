 cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)




project(smartheat 
        VERSION 0.1
        DESCRIPTION "Smart Heat"
        LANGUAGES C)



add_library(project_warning INTERFACE)


target_compile_options(project_warning INTERFACE -Wall -Wextra -Wpedantic -Werror)


add_subdirectory(ports)

add_subdirectory(hal)
add_subdirectory(domain)
add_subdirectory(adapters)

add_executable(smartheat main.c)

target_link_libraries(smartheat PRIVATE hal domain adapters ports project_warning)


include (CTest)
if (BUILD_TESTING)
    add_subdirectory(tests)
endif()
