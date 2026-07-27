cmake_minimum_required(VERSION 3.16)
project(TomasuloCPUSimulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR})

add_executable(interpreter
    src/interpreter_exec.cpp
    src/interpreter/interpreter.cpp
    src/interpreter/regfile.cpp
    src/interpreter/tracefile.cpp
    src/mem/datamem.cpp
    src/mem/instrmem.cpp
    src/mem/simmem.cpp
    src/ints.cpp
    src/decoder/instr.cpp
    src/alu/alucomp.cpp
)

set_directory_properties(PROPERTIES ADDITIONAL_CLEAN_FILES "*.trace")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit_tests/CMakeLists.txt")
    option(BUILD_TESTS "Build unit tests" ON)
    if(BUILD_TESTS)
        enable_testing()
        add_subdirectory(unit_tests)
    endif()
endif()