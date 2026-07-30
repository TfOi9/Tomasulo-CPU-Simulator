add_executable(test_single_cdb
    tomasulo/test_single_cdb.cpp
    ${CMAKE_SOURCE_DIR}/src/tomasulo/alurs.cpp
    ${CMAKE_SOURCE_DIR}/src/tomasulo/datamem.cpp
    ${CMAKE_SOURCE_DIR}/src/tomasulo/lsrs.cpp
    ${CMAKE_SOURCE_DIR}/src/tomasulo/regaliastab.cpp
    ${CMAKE_SOURCE_DIR}/src/tomasulo/regfile.cpp
    ${CMAKE_SOURCE_DIR}/src/tomasulo/storebuf.cpp
    ${CMAKE_SOURCE_DIR}/src/mem/datamem.cpp
    ${CMAKE_SOURCE_DIR}/src/mem/simmem.cpp
    ${CMAKE_SOURCE_DIR}/src/alu/alucomp.cpp
    ${CMAKE_SOURCE_DIR}/src/ints.cpp
)

add_test(NAME test_single_cdb COMMAND test_single_cdb
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
