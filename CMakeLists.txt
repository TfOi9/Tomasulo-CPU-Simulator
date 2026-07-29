cmake_minimum_required(VERSION 3.16)
project(TomasuloCPUSimulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

add_library(tomasulo STATIC
    src/tomasulo/cpu.cpp
    src/tomasulo/alurs.cpp
    src/tomasulo/branchpred.cpp
    src/tomasulo/datamem.cpp
    src/tomasulo/lsrs.cpp
    src/tomasulo/regaliastab.cpp
    src/tomasulo/regfile.cpp
    src/tomasulo/reorderbuf.cpp
    src/tomasulo/storebuf.cpp
    src/tomasulo/tracefile.cpp
)

add_executable(simulator
    src/main.cpp
    src/tomasulo/cpu.cpp
    src/tomasulo/alurs.cpp
    src/tomasulo/branchpred.cpp
    src/tomasulo/datamem.cpp
    src/tomasulo/lsrs.cpp
    src/tomasulo/regaliastab.cpp
    src/tomasulo/regfile.cpp
    src/tomasulo/reorderbuf.cpp
    src/tomasulo/storebuf.cpp
    src/tomasulo/tracefile.cpp
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