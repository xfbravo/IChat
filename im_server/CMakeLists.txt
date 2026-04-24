cmake_minimum_required(VERSION 3.10)
project(im_server VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")

# ============================================
# Boost 库查找
# ============================================
find_package(Boost REQUIRED COMPONENTS system thread)

if(Boost_FOUND)
    message(STATUS "Found Boost: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost version: ${Boost_VERSION}")
else()
    message(FATAL_ERROR "Boost not found! Please install libboost-all-dev or boost libraries")
endif()

# ============================================
# MySQL 客户端库查找
# ============================================
find_package(MySQL)

if(MySQL_FOUND)
    message(STATUS "Found MySQL: ${MySQL_INCLUDE_DIR}")
else()
    message(STATUS "MySQL not found in CMake, will try pkg-config")
    # 尝试使用 pkg_config
    include(FindPkgConfig)
    if(pkg_config_found)
        pkg_check_modules(MySQL REQUIRED mysqlclient)
    endif()
endif()

# 源文件
set(SOURCES
    src/main.cpp
    src/server/server.cpp
    src/session/session.cpp
    src/protocol/codec.cpp
    src/thread/thread_pool.cpp
    src/dispatcher/dispatcher.cpp
    src/db/db_pool.cpp
    src/db/user_service.cpp
)

# 头文件
set(HEADERS
    src/server/server.h
    src/session/session.h
    src/protocol/message.h
    src/protocol/codec.h
    src/thread/thread_pool.h
    src/dispatcher/dispatcher.h
    src/db/db_pool.h
    src/db/user_service.h
)

# 可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# 链接库
target_link_libraries(${PROJECT_NAME}
    Boost::system
    Boost::thread
    mysqlclient
)

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE src)

# 安装目标
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
