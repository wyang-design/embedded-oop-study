


add_library(adapters lm75_i2c.c uart_status.c gpio_heater.c)


target_include_directories(adapters PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(adapters PUBLIC ports PRIVATE hal project_warning)
