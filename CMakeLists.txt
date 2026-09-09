cmake_minimum_required(VERSION 3.16)

project(ExistOS-Dump-Bridge VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CTest)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(edb
    edb/EDBInterface.cpp
    edb/EDBUtils.cpp
    edb/main.cpp
)

target_include_directories(edb PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/edb
)

if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(edb_unit_tests
        tests/EDBUtilsTests.cpp
        tests/EDBInterfaceTests.cpp
        tests/EDBTransportFactoryStub.cpp
        edb/EDBInterface.cpp
        edb/EDBUtils.cpp
    )
    target_include_directories(edb_unit_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/edb
    )
    target_link_libraries(edb_unit_tests PRIVATE GTest::gtest_main)
    add_test(NAME edb_unit_tests COMMAND edb_unit_tests)
endif()

if(APPLE)
    target_sources(edb PRIVATE edb/EDBTransportMac.cpp edb/EDBSerialPosix.cpp)
    target_compile_definitions(edb PRIVATE EDB_MACOS=1)
    find_library(DISKARBITRATION_FRAMEWORK DiskArbitration REQUIRED)
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    target_link_libraries(edb PRIVATE
        ${DISKARBITRATION_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${COREFOUNDATION_FRAMEWORK}
    )
elseif(UNIX)
    target_sources(edb PRIVATE edb/EDBTransportUnix.cpp edb/EDBSerialPosix.cpp)
    target_compile_definitions(edb PRIVATE EDB_LINUX=1)
elseif(WIN32)
    target_sources(edb PRIVATE edb/EDBTransportWindows.cpp edb/EDBWinReg.cpp edb/CComHelper.cpp)
    target_compile_definitions(edb PRIVATE UNICODE _UNICODE)
    target_compile_options(edb PRIVATE /utf-8)
    target_link_libraries(edb PRIVATE Advapi32)
endif()

install(TARGETS edb
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES README.md LICENSE
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
