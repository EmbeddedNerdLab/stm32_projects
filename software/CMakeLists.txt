cmake_minimum_required(VERSION 3.20)
project(STM32Monitor VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Charts SerialPort Sql)

add_executable(STM32Monitor WIN32
    main.cpp
    mainwindow.cpp
    serialworker.cpp
    dataparser.cpp
    database.cpp
    widgets.cpp
    flashworker.cpp
    flashdialog.cpp
)

target_link_libraries(STM32Monitor PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Charts
    Qt6::SerialPort
    Qt6::Sql
)

# Copy style.qss next to the executable after build
add_custom_command(TARGET STM32Monitor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/style.qss
        $<TARGET_FILE_DIR:STM32Monitor>/style.qss
)
