cmake_minimum_required(VERSION 3.7)
project(RGBStripController C)

# Put binaries in their own directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Always compile with strict warnings
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

add_subdirectory("src")
