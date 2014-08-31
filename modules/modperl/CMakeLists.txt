include(${SWIG_USE_FILE})

include_directories(${PERL_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set_source_files_properties(modperl.i PROPERTIES CPLUSPLUS ON)
set_source_files_properties(modperl.i PROPERTIES SWIG_FLAGS "-w362,315,401,402")

add_custom_target(perlfunctions
                  COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/codegen.pl
                                             ${CMAKE_CURRENT_SOURCE_DIR}/functions.in
                                             ${CMAKE_CURRENT_BINARY_DIR}/functions.cpp)

add_custom_target(swigperlrun
                  COMMAND ${SWIG_EXECUTABLE} -perl5 -c++ -shadow -external-runtime
                                             ${CMAKE_CURRENT_BINARY_DIR}/swigperlrun.h)

set(SWIG_MODULE_ZNC_EXTRA_DEPS perlfunctions swigperlrun)

swig_add_module(ZNC perl5 modperl.i modperl.cpp)
swig_link_libraries(ZNC ${ZNC_LIBRARIES} ${PERL_LIBRARIES})
set_target_properties(${SWIG_MODULE_ZNC_REAL_NAME} PROPERTIES PREFIX "")
if (APPLE)
    set_target_properties(${SWIG_MODULE_ZNC_REAL_NAME} PROPERTIES SUFFIX .bundle)
endif()

add_library(modperl MODULE modperl.cpp)
add_dependencies(modperl perlfunctions swigperlrun)
target_link_libraries(modperl ${ZNC_LIBRARIES} ${PERL_LIBRARIES})
set_target_properties(modperl PROPERTIES PREFIX "" SUFFIX .so LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/modules)

set(MODPERL_FLAGS "-Wno-write-strings -Wno-redundant-decls -Wno-missing-declarations")
set(MODPERL_FLAGS "${MODPERL_FLAGS} -Wno-type-limits -Wno-sign-compare -Wno-strict-overflow -Wno-unused-value")
# perl 5.20 will fix this warning: https://rt.perl.org/Public/Bug/Display.html?id=120670
set(MODPERL_FLAGS "${MODPERL_FLAGS} -Wno-reserved-user-defined-literal -Wno-literal-suffix")
set(MODPERL_FLAGS "${MODPERL_FLAGS} -Wno-duplicate-decl-specifier -Wno-deprecated-register")
set(MODPERL_FLAGS "${MODPERL_FLAGS} -DSWIG_TYPE_TABLE=znc -DEXPORT_ZNC")

set_source_files_properties(${swig_generated_file_fullname} PROPERTIES COMPILE_FLAGS ${MODPERL_FLAGS})
set_source_files_properties(modperl.cpp PROPERTIES COMPILE_FLAGS ${MODPERL_FLAGS})

# deploy to build dir
configure_file(startup.pl startup.pl COPYONLY)

if(NOT DEVELOPER_BUILD)
    install(TARGETS modperl LIBRARY DESTINATION ${INSTALL_MODDIR})
    install(TARGETS ${SWIG_MODULE_ZNC_REAL_NAME} LIBRARY DESTINATION ${INSTALL_MODDIR}/modperl)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/startup.pl
                  ${CMAKE_CURRENT_BINARY_DIR}/ZNC.pm DESTINATION ${INSTALL_MODDIR}/modperl)
endif()
