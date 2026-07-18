cmake_minimum_required(VERSION 3.20)
project(msi_fb)

set(CMAKE_CXX_STANDARD 20)

add_executable(${PROJECT_NAME} WIN32
    main.cpp
    config.h
    pawnio_ec.cpp
    pawnio_ec.h assets/app.rc)

target_link_libraries(${PROJECT_NAME} PRIVATE
    shell32
    user32)

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:-O0 -g>
    $<$<CONFIG:Release>:-O3 -s -DNDEBUG -ffunction-sections -fdata-sections>
)

target_link_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Release>:-Wl,--gc-sections>
)
