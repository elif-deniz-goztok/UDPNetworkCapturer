cmake_minimum_required(VERSION 3.30)
project(UDPNetworkCapturer)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)

# Statically link GCC/MinGW C++ runtime libraries for a standalone .exe
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -static-libgcc -static-libstdc++ -static")

# Add main executable
add_executable(UDPNetworkCapturer WIN32 main.cpp)

# Link necessary Windows libraries
target_link_libraries(UDPNetworkCapturer PRIVATE iphlpapi ws2_32)
