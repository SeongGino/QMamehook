cmake_minimum_required(VERSION 3.14)

project(QMamehook LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT QMH_QT_VERSIONS)
    set(QMH_QT_VERSIONS Qt6)
endif()

option(QMH_VERSION "Sets QMamehook version number" OFF)
option(QMH_GITHASH "Sets QMamehook git hash" OFF)

if(QMH_VERSION)
    add_compile_definitions(QMH_VERSION="${QMH_VERSION}")
endif()

if(QMH_GITHASH)
    add_compile_definitions(QMH_GITHASH="${QMH_GITHASH}")
endif()

find_package(QT NAMES ${QMH_QT_VERSIONS} REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

add_executable(QMamehook
  main.cpp
  qhookermain.h
  qhookermain.cpp
)
target_link_libraries(QMamehook PRIVATE Qt${QT_VERSION_MAJOR}::Core)

# TCP
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network)
target_link_libraries(QMamehook PRIVATE Qt${QT_VERSION_MAJOR}::Network)

# Serial Port
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS SerialPort)
target_link_libraries(QMamehook PRIVATE Qt${QT_VERSION_MAJOR}::SerialPort)

include(GNUInstallDirs)
install(TARGETS QMamehook
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
