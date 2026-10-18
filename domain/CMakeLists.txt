add_library(domain thermostat.c)


target_include_directories(domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(domain PUBLIC ports PRIVATE  project_warning)