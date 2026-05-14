cmake_minimum_required(VERSION 3.16)
project(im_client VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 所需的选项
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找 Qt6
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Network
    Widgets
    Gui
    Multimedia
    Qml
    Quick
    QuickControls2
)

# 源文件
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow_messages.cpp
    src/mainwindow_contacts.cpp
    src/mainwindow_me.cpp
    src/mainwindow_moments.cpp
    src/loginwindow.cpp
    src/tcpclient.cpp
    src/protocol.cpp
    src/addcontactdialog.cpp
)

# 头文件
set(HEADERS
    include/mainwindow.h
    include/mainwindow_helpers.h
    include/loginwindow.h
    include/tcpclient.h
    include/protocol.h
    include/addcontactdialog.h
)

# 添加 include 目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 资源文件
set(RESOURCES
    res.qrc
)

# 可执行文件
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

# 链接 Qt 库
target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Network
    Qt6::Widgets
    Qt6::Gui
    Qt6::Multimedia
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

# Windows 特定配置
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE ON
    )
endif()
