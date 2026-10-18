

add_executable(test_thermostat test_thermostat.c)

target_link_libraries(test_thermostat PRIVATE domain ports project_warning)


add_test(NAME thermostat COMMAND test_thermostat)
