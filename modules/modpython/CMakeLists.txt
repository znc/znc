include(${SWIG_USE_FILE})

include_directories(${PYTHON_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set_source_files_properties(modpython.i PROPERTIES CPLUSPLUS ON)
set_source_files_properties(modpython.i PROPERTIES SWIG_FLAGS "-w362,315,401,402")

add_custom_target(pyfunctions
                  COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/codegen.pl
                                             ${CMAKE_CURRENT_SOURCE_DIR}/functions.in
                                             ${CMAKE_CURRENT_BINARY_DIR}/functions.cpp)

add_custom_target(swigpyrun
                  COMMAND ${SWIG_EXECUTABLE} -python -py3 -c++ -shadow -external-runtime
                                             ${CMAKE_CURRENT_BINARY_DIR}/swigpyrun.h)

set(SWIG_MODULE_znc_core_EXTRA_DEPS pyfunctions swigpyrun)

swig_add_module(znc_core python modpython.i modpython.cpp)
swig_link_libraries(znc_core ${ZNC_LIBRARIES} ${PYTHON_LIBRARIES})
set_target_properties(${SWIG_MODULE_znc_core_REAL_NAME} PROPERTIES PREFIX "" SUFFIX .so)

add_library(modpython MODULE modpython.cpp)
add_dependencies(modpython pyfunctions swigpyrun)
target_link_libraries(modpython ${ZNC_LIBRARIES} ${PYTHON_LIBRARIES})
set_target_properties(modpython PROPERTIES PREFIX "" SUFFIX .so LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/modules)

set(MODPYTHON_FLAGS "-Wno-missing-field-initializers -Wno-unused -Wno-shadow")
set(MODPYTHON_FLAGS "${MODPYTHON_FLAGS} -Wno-missing-declarations -Wno-uninitialized -Wno-switch-enum")
set(MODPYTHON_FLAGS "${MODPYTHON_FLAGS} -Wno-redundant-decls -Wno-parentheses-equality")
set(MODPYTHON_FLAGS "${MODPYTHON_FLAGS} -DSWIG_TYPE_TABLE=znc")

set_source_files_properties(${swig_generated_file_fullname} PROPERTIES COMPILE_FLAGS ${MODPYTHON_FLAGS})
set_source_files_properties(modpython.cpp PROPERTIES COMPILE_FLAGS ${MODPYTHON_FLAGS})

# deploy to build dir
configure_file(znc.py znc.py COPYONLY)

if(NOT DEVELOPER_BUILD)
    install(TARGETS modpython LIBRARY DESTINATION ${INSTALL_MODDIR})
    install(TARGETS ${SWIG_MODULE_znc_core_REAL_NAME} LIBRARY DESTINATION ${INSTALL_MODDIR}/modpython)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/znc_core.py
                  ${CMAKE_CURRENT_SOURCE_DIR}/znc.py DESTINATION ${INSTALL_MODDIR}/modpython)
endif()
