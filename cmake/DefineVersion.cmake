if(NOT DEFINED ZNC_VERSION_MAJOR OR NOT DEFINED ZNC_VERSION_MINOR)
    message(FATAL_ERROR "ZNC_VERSION_MINOR and ZNC_VERSION_MAJOR must be defined in the beginning of the root CMakeLists.txt.")
endif()

if(NOT DEFINED ZNC_VERSION_EXTRA)
    set(ZNC_VERSION_EXTRA $ENV{ZNC_VERSION_EXTRA})
    if(NOT ZNC_VERSION_EXTRA)
        if(GIT_EXECUTABLE)
            execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
                            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                            RESULT_VARIABLE GIT_RES
                            OUTPUT_VARIABLE GIT_SHA1
                            OUTPUT_STRIP_TRAILING_WHITESPACE)
            if(NOT GIT_RES)
                set(ZNC_VERSION_EXTRA "git-${GIT_SHA1}")
            endif()
        endif()
    endif()
endif()

if(ZNC_VERSION_EXTRA)
    set(ZNC_VERSION ${ZNC_VERSION_MAJOR}.${ZNC_VERSION_MINOR}-${ZNC_VERSION_EXTRA})
else()
    set(ZNC_VERSION ${ZNC_VERSION_MAJOR}.${ZNC_VERSION_MINOR})
endif()

if(ZNC_VERSION_INPUT AND ZNC_VERSION_OUTPUT)
    configure_file(${ZNC_VERSION_INPUT} ${ZNC_VERSION_OUTPUT})
else()
    add_custom_target(version ${CMAKE_COMMAND}
                  -D GIT_EXECUTABLE=${GIT_EXECUTABLE}
                  -D PROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
                  -D ZNC_VERSION_MAJOR=${ZNC_VERSION_MAJOR}
                  -D ZNC_VERSION_MINOR=${ZNC_VERSION_MINOR}
                  -D ZNC_VERSION_INPUT=${PROJECT_SOURCE_DIR}/src/version.cpp.in
                  -D ZNC_VERSION_OUTPUT=${PROJECT_BINARY_DIR}/src/version.cpp
                  -P ${PROJECT_SOURCE_DIR}/cmake/DefineVersion.cmake)
endif()
