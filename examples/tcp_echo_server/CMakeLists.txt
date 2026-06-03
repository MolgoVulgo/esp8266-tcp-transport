cmake_minimum_required(VERSION 3.5)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../.."
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tcp_echo_server)
