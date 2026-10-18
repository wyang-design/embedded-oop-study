

add_executable(test_thermostat test_thermostat.c)

target_link_libraries(test_thermostat PRIVATE domain ports ports)


add_test(NAME thermostat COMMAND test_thermostat)