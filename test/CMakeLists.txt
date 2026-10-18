cmake_minimum_required(VERSION 3.20)


project(test_thermostat
    VERSION 0.1
    DESCRIPTION "Test Thermostat"
    LANGUAGES C
    )


add_executable(test_thermostat test_thermostat.c ../domain/thermostat.c)

target_include_directories(test_thermostat PRIVATE ../domain
    )