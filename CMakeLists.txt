cmake_minimum_required(VERSION 3.13)
project(Lucix)

set(CMAKE_CXX_STANDARD 17)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

file(GLOB SOURCES gen/*.cpp)

# find_package(ANTLR4 REQUIRED)

include_directories(/usr/include/antlr4-runtime/)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs support core irreader
        passes orcjit native)


add_executable(Lucix main.cpp ${SOURCES})
target_link_libraries(Lucix
        ${llvm_libs}
        "-lantlr4-runtime")