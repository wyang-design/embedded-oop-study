


add_library(adapters lm75_i2c.c)


target_include_directories(adapters PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(adapters PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../ )