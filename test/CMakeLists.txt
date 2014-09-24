if(NOT DEFINED GTEST_VERSION)
    set(GTEST_VERSION "1.7.0")
endif()
if(NOT DEFINED GTEST_DIR)
    set(GTEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/gtest-${GTEST_VERSION}")
endif()
if(NOT DEFINED GTEST_ZIP)
    set(GTEST_ZIP "${CMAKE_CURRENT_BINARY_DIR}/gtest.zip")
endif()
if(NOT DEFINED GTEST_URL)
    set(GTEST_URL "http://googletest.googlecode.com/files/gtest-${GTEST_VERSION}.zip")
endif()

# make absolute for 'if exists'
get_filename_component(GTEST_ZIP ${GTEST_ZIP} ABSOLUTE)
get_filename_component(GTEST_DIR ${GTEST_DIR} ABSOLUTE)

find_package(Wget QUIET)
if(EXISTS ${GTEST_ZIP} OR EXISTS ${GTEST_DIR} OR WGET_FOUND)
    if(NOT EXISTS ${GTEST_DIR})
        if (NOT EXISTS ${GTEST_ZIP})
            add_custom_command(OUTPUT ${GTEST_ZIP}
                               COMMAND ${WGET_EXECUTABLE} -q ${GTEST_URL} -O ${GTEST_ZIP}
                               COMMENT "Downloading GoogleTest ${GTEST_VERSION}...")
        endif()
        add_custom_command(OUTPUT ${GTEST_DIR}/src/gtest-all.cc ${GTEST_DIR}/src/gtest_main.cc
                           DEPENDS ${GTEST_ZIP}
                           COMMAND unzip -qo ${GTEST_ZIP}
                           COMMENT "Unpacking GoogleTest ${GTEST_VERSION}...")
    endif()

    include_directories(${GTEST_DIR} ${GTEST_DIR}/include)
    add_executable(unittest EXCLUDE_FROM_ALL
                            ConfigTest.cpp
                            StringTest.cpp
                            ThreadTest.cpp
                            UtilsTest.cpp
                            ${GTEST_DIR}/src/gtest-all.cc
                            ${GTEST_DIR}/src/gtest_main.cc)
    target_link_libraries(unittest ${ZNC_LIBRARIES})

    # target "test" is reserved
    if(POLICY CMP0037)
        cmake_policy(SET CMP0037 OLD)
    endif()

    # add_test() eats the output
    add_custom_target(test COMMAND unittest DEPENDS unittest)
endif()
