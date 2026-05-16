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

# ============================================================
# libdatachannel / optional native WebRTC experiments
#
# 注意：
# 1. The primary audio/video call path uses an external browser page
#    served by a local Qt HTTP bridge. libdatachannel is optional and only
#    kept for the old native signaling adapter.
#
# 2. 这里应指向 cmake --install build 之后的安装目录：
#       C:/deps/libdatachannel
#    不要指向源码目录：
#       C:/msys64/home/21023/libdatachannel
#    也不要指向 build 目录。
#
# 3. 你的 Qt 工程使用的是 Qt 自带 MinGW：
#       D:/QT/Tools/mingw1310_64
#    所以 libdatachannel 也必须用同一套 MinGW 编译并安装。
# ============================================================
option(ICHAT_WITH_LIBDATACHANNEL "Enable optional libdatachannel signaling adapter" OFF)
set(ICHAT_LIBDATACHANNEL_ROOT "C:/deps/libdatachannel" CACHE PATH
    "Root directory of the Windows/MinGW libdatachannel install"
)

if(ICHAT_WITH_LIBDATACHANNEL)
    if(NOT EXISTS "${ICHAT_LIBDATACHANNEL_ROOT}")
        message(WARNING
            "ICHAT_LIBDATACHANNEL_ROOT does not exist: ${ICHAT_LIBDATACHANNEL_ROOT}\n"
            "Please run: cmake --install build"
        )
    endif()

    # 让 find_package 优先从安装目录查找 LibDataChannelConfig.cmake
    list(PREPEND CMAKE_PREFIX_PATH "${ICHAT_LIBDATACHANNEL_ROOT}")

    find_package(LibDataChannel CONFIG QUIET)

    if(NOT LibDataChannel_FOUND)
        message(STATUS "libdatachannel not found: optional native adapter disabled")
    endif()

    if(LibDataChannel_FOUND AND TARGET LibDataChannel::LibDataChannel)
        set(ICHAT_LIBDATACHANNEL_TARGET LibDataChannel::LibDataChannel)
    elseif(LibDataChannel_FOUND AND TARGET datachannel)
        set(ICHAT_LIBDATACHANNEL_TARGET datachannel)
    elseif(LibDataChannel_FOUND)
        message(FATAL_ERROR
            "LibDataChannel was found, but no usable imported target exists. "
            "Expected target: LibDataChannel::LibDataChannel or datachannel."
        )
    endif()

    if(LibDataChannel_FOUND)
        message(STATUS "Found libdatachannel: ${ICHAT_LIBDATACHANNEL_ROOT}")
        message(STATUS "Using libdatachannel target: ${ICHAT_LIBDATACHANNEL_TARGET}")
    endif()
endif()

# 源文件
set(SOURCES
    src/callbrowserbridge.cpp
    src/callmediaadapter.cpp
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow_messages.cpp
    src/mainwindow_contacts.cpp
    src/mainwindow_me.cpp
    src/mainwindow_moments.cpp
    src/mainwindow_calls.cpp
    src/loginwindow.cpp
    src/tcpclient.cpp
    src/protocol.cpp
    src/addcontactdialog.cpp
)

# 头文件
set(HEADERS
    include/callbrowserbridge.h
    include/callmediaadapter.h
    include/mainwindow.h
    include/mainwindow_helpers.h
    include/loginwindow.h
    include/tcpclient.h
    include/protocol.h
    include/addcontactdialog.h
)

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

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/web"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/web"
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# 链接 Qt 库
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Widgets
        Qt6::Gui
        Qt6::Multimedia
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)

if(ICHAT_WITH_LIBDATACHANNEL AND LibDataChannel_FOUND)
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            ${ICHAT_LIBDATACHANNEL_TARGET}
    )

    target_compile_definitions(${PROJECT_NAME}
        PRIVATE
            ICHAT_WITH_LIBDATACHANNEL=1
    )
endif()

# Windows 特定配置
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE ON
    )
endif()
