add_library(hal hal_pc.c)


target_include_directories(hal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(hal PRIVATE project_warning)
