cmake_minimum_required(VERSION 2.8.4)
project(ZNC)

set(ZNC_VERSION_MAJOR 1)
set(ZNC_VERSION_MINOR 5)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(DefineFeatures)
include(DefineDirectories)
include(DefineBuildFlags)
include(DefineVersion)

### SOURCES

list(APPEND ZNC_SOURCES src/Buffer.cpp
                        src/Chan.cpp
                        src/Client.cpp
                        src/ClientCommand.cpp
                        src/Config.cpp
                        src/Csocket.cpp
                        src/FileUtils.cpp
                        src/HTTPSock.cpp
                        src/IRCNetwork.cpp
                        src/IRCSock.cpp
                        src/Listener.cpp
                        src/MD5.cpp
                        src/Modules.cpp
                        src/Nick.cpp
                        src/Query.cpp
                        src/SHA256.cpp
                        src/Server.cpp
                        src/Socket.cpp
                        src/Template.cpp
                        src/Threads.cpp
                        src/User.cpp
                        src/Utils.cpp
                        src/WebModules.cpp
                        src/ZNCDebug.cpp
                        src/ZNCString.cpp
                        src/znc.cpp
                        ${PROJECT_BINARY_DIR}/src/version.cpp
)

### TARGETS

add_library(libznc SHARED ${ZNC_SOURCES})
include_directories(${ZNC_INCLUDEDIRS})
target_link_libraries(libznc ${ZNC_LIBRARIES})
set_target_properties(libznc PROPERTIES OUTPUT_NAME znc)
add_dependencies(libznc version)

list(APPEND ZNC_LIBRARIES libznc)

add_executable(znc src/main.cpp)
target_link_libraries(znc ${ZNC_LIBRARIES})

add_subdirectory(modules)
add_subdirectory(share)
add_subdirectory(test)

### INSTALLS

if(NOT DEVELOPER_BUILD)
    install(TARGETS znc RUNTIME DESTINATION ${INSTALL_BINDIR})
    install(TARGETS libznc LIBRARY DESTINATION ${INSTALL_LIBDIR}
                           RUNTIME DESTINATION ${INSTALL_BINDIR}
                           ARCHIVE DESTINATION ${INSTALL_LIBDIR})
    install(DIRECTORY include/znc DESTINATION ${INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN *.h)
endif()

### GENERATED FILES

configure_file("${PROJECT_SOURCE_DIR}/include/znc/zncconfig.h.in"
               "${PROJECT_BINARY_DIR}/include/znc/zncconfig.h")
configure_file("${PROJECT_SOURCE_DIR}/include/znc/version.h.in"
               "${PROJECT_BINARY_DIR}/include/znc/version.h")
configure_file("${PROJECT_SOURCE_DIR}/src/version.cpp.in"
               "${PROJECT_BINARY_DIR}/src/version.cpp")

if(NOT DEVELOPER_BUILD)
    install(FILES "${PROJECT_BINARY_DIR}/include/znc/zncconfig.h"
            DESTINATION ${INSTALL_INCLUDEDIR}/znc)
    install(FILES "${PROJECT_BINARY_DIR}/include/znc/version.h"
            DESTINATION ${INSTALL_INCLUDEDIR}/znc)
endif()

process_definitions("${ZNC_DEFINITIONS}" ZNC_DEFINITION_FLAGS)
process_includedirs("${ZNC_INCLUDEDIRS}" ZNC_INCLUDE_FLAGS)
process_link_libraries("${ZNC_LIBRARIES}" ZNC_LIBRARY_FLAGS)

if(NOT DEVELOPER_BUILD)
    set(ZNC_INCLUDE_FLAGS "-I${INSTALL_INCLUDEDIR} ${ZNC_INCLUDE_FLAGS}")
    set(ZNC_LIBRARY_FLAGS "-L${INSTALL_LIBDIR} ${ZNC_LIBRARY_FLAGS}")
else()
    set(ZNC_INCLUDE_FLAGS "-I${PROJECT_SOURCE_DIR}/include -I${PROJECT_BINARY_DIR}/include ${ZNC_INCLUDE_FLAGS}")
    set(ZNC_LIBRARY_FLAGS "-L${PROJECT_BINARY_DIR} ${ZNC_LIBRARY_FLAGS}")
endif()

set(ZNC_CXXFLAGS "${CMAKE_SHARED_MODULE_CXX_FLAGS} ${CMAKE_CXX_FLAGS} ${ZNC_DEFINITION_FLAGS} ${ZNC_INCLUDE_FLAGS}")
set(ZNC_LDFLAGS "${CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS} ${ZNC_LIBRARY_FLAGS}")

configure_file("${PROJECT_SOURCE_DIR}/znc-buildmod.in"
               "${PROJECT_BINARY_DIR}/znc-buildmod" @ONLY)

configure_file("${PROJECT_SOURCE_DIR}/znc.pc.in"
               "${PROJECT_BINARY_DIR}/znc.pc" @ONLY)

if(NOT DEVELOPER_BUILD)
    install(FILES "${PROJECT_BINARY_DIR}/znc.pc" DESTINATION ${INSTALL_LIBDIR}/pkgconfig)
    install(PROGRAMS "${PROJECT_BINARY_DIR}/znc-buildmod" DESTINATION ${INSTALL_BINDIR})
endif()

### PACKAGE

if(NOT DEVELOPER_BUILD)
    if(NOT CPACK_SOURCE_PACKAGE_FILE_NAME)
        set(CPACK_SOURCE_PACKAGE_FILE_NAME "znc-${ZNC_VERSION}")
    endif()
    if(NOT CPACK_OUTPUT_FILE_PREFIX)
        set(CPACK_OUTPUT_FILE_PREFIX ${PROJECT_BINARY_DIR})
    endif()
    set(CPACK_PACKAGE_NAME znc)
    set(CPACK_PACKAGE_VENDOR "http://znc.in/")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "ZNC - an advanced IRC bouncer")
    set(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/README.md")
    set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
    set(CPACK_PACKAGE_VERSION_MAJOR ${ZNC_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${ZNC_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${ZNC_VERSION_EXTRA})
    set(CPACK_PACKAGE_VERSION ${ZNC_VERSION})
    set(CPACK_GENERATOR TGZ)
    set(CPACK_SOURCE_GENERATOR TGZ)
    set(CPACK_STRIP_FILES "znc")
    set(CPACK_PACKAGE_EXECUTABLES "znc" "ZNC IRC bouncer")
    set(CPACK_SOURCE_IGNORE_FILES "/.git;Makefile;CMakeFiles;CMakeCache.txt;CPackConfig.cmake;CPackSourceConfig.cmake;cmake_install.cmake;/*.log;/.travis*;/*.gz;make-tarball.sh")
    include(CPack)

    set(ZNC_TARBALL "${CPACK_OUTPUT_FILE_PREFIX}/${CPACK_SOURCE_PACKAGE_FILE_NAME}.tar.gz")
    add_custom_target(package_sign COMMAND gpg --detach-sign ${ZNC_TARBALL} COMMENT "Signing ${ZNC_TARBALL}...")
endif()

### SUMMARY

set(BUILD_TYPE ${CMAKE_BUILD_TYPE})
if(DEVELOPER_BUILD)
    set(BUILD_TYPE "${BUILD_TYPE} (developer)")
endif()

message(STATUS "")
message(STATUS "${PROJECT_NAME} ${ZNC_VERSION} configuration:")
message(STATUS "")
if(NOT DEVELOPER_BUILD)
    message(STATUS "Prefix ............... ${INSTALL_PREFIX}")
endif()
message(STATUS "Build ................ ${BUILD_TYPE}")
message(STATUS "IPv6 ................. ${HAVE_IPV6}")
message(STATUS "SSL .................. ${HAVE_LIBSSL}")
message(STATUS "ICU .................. ${HAVE_ICU}")
message(STATUS "Zlib ................. ${HAVE_ZLIB}")
message(STATUS "Cyrus ................ ${HAVE_CYRUS}")
message(STATUS "Threads .............. ${HAVE_THREADS}")
message(STATUS "SWIG ................. ${HAVE_SWIG}")
message(STATUS "  Perl ............... ${HAVE_PERL}")
message(STATUS "  Python ............. ${HAVE_PYTHON}")
message(STATUS "TCL .................. ${HAVE_TCL}")
message(STATUS "")
