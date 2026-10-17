 cmake_minimum_required(VERSION 3.20)



set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

 project(smartheat 
    VERSION 0.1
    DESCRIPTION "Smart Heat"
    LANGUAGES C)


 add_executable(smartheat main.c hal/hal_pc.c domain/thermostat.c)


 target_include_directories(smartheat PRIVATE hal 
    domain
    )

