cmake_minimum_required(VERSION 3.16)
project(CanBusSimulatorInstaller VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "D:/Qt/6.11.1/mingw_64")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

qt_add_executable(CanBusSimulatorInstaller
    main.cpp
    installerwizard.cpp installerwizard.h
    installerutils.cpp installerutils.h
    payload.qrc
)

target_link_libraries(CanBusSimulatorInstaller PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)

if(WIN32)
    target_link_libraries(CanBusSimulatorInstaller PRIVATE ole32 uuid)
    if(MINGW)
        target_link_options(CanBusSimulatorInstaller PRIVATE "-mwindows")
    endif()
endif()


