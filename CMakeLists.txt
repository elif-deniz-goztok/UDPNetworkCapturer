cmake_minimum_required(VERSION 3.30)
project(UDPNetworkCapturer)

set(CMAKE_CXX_STANDARD 11)

add_executable(UDPNetworkCapturer main.cpp)

target_link_libraries(UDPNetworkCapturer PRIVATE ws2_32)
