cmake_minimum_required(VERSION 3.16)
project(CanBusSimulator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# NOT: Eğer başka bir bilgisayarda CMake Qt'yi bulamazsa, aşağıdaki yorum satırını kaldırıp 
# kendi bilgisayarınızdaki Qt yolunu yazabilirsiniz. (Github için yoruma alındı)
# set(CMAKE_PREFIX_PATH "D:/Qt/6.11.1/mingw_64")
# Qt6 ve Widgets modülünü buluyoruz
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql)
add_executable(CanBusSimulator WIN32
    main.cpp
    mainwindow.cpp
    mainwindow.h
    updater.cpp
    updater.h
    dbcparser.cpp
    dbcparser.h
    app_icon.rc
    dbmanager.cpp
    dbmanager.h
    codec8builder.cpp
    codec8builder.h
    teltonikatcpclient.cpp
    teltonikatcpclient.h
)
target_link_libraries(CanBusSimulator PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Sql)

qt_standard_project_setup()
